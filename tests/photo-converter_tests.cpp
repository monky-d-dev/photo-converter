#include "backend.h"
#include "appearance.h"
#include "previewprovider.h"
#include <QColorSpace>
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QPainter>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

class PhotoConverterTests : public QObject {
    Q_OBJECT
    QString fixture(const QString &name) const {
        return QFINDTESTDATA(QStringLiteral("fixtures/") + name);
    }
    QString imageFile(const QString &path, QColor color = Qt::blue, QSize size = {120, 180}) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QImage image(size, QImage::Format_RGB32); image.fill(color);
        if (!image.save(path)) return {};
        return path;
    }
    CancelFlag flag() const { return std::make_shared<std::atomic_bool>(false); }
    QPoint point(QQuickItem *item, QPointF local = {}) {
        if (local.isNull()) local = {item->width() / 2, item->height() / 2};
        return item->mapToScene(local).toPoint();
    }
    QQuickItem *findItem(QQuickItem *root, const QString &name) {
        if (root->objectName() == name) return root;
        for (auto *child : root->childItems()) if (auto *found = findItem(child, name)) return found;
        return nullptr;
    }
    double contrast(const QColor &a, const QColor &b) {
        auto luminance = [](const QColor &c) {
            auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
            return .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
        };
        const double x = luminance(a), y = luminance(b);
        return (qMax(x, y) + .05) / (qMin(x, y) + .05);
    }

private slots:
    void initTestCase() { QQuickStyle::setStyle("Basic"); }

    void appearanceRemembersChosenTheme() {
        QTemporaryDir dir;
        const auto path = dir.filePath("appearance.ini");
        Appearance first(path); QCOMPARE(first.theme(), QString("light"));
        first.setTheme("dark"); QVERIFY(first.dark());
        Appearance restored(path); QCOMPARE(restored.theme(), QString("dark"));
        restored.setTheme("invalid"); QVERIFY(restored.dark());
        restored.setTheme("light");
        Appearance light(path); QVERIFY(!light.dark());
    }

    void marginPreferenceSurvivesRestartAndUndo() {
        QTemporaryDir dir;
        const auto path = dir.filePath("preferences.ini");
        {
            Appearance preferences(path); preferences.setTheme("dark");
            Backend backend(nullptr, &preferences);
            QCOMPARE(backend.marginMm(), .5);
            backend.setMarginMm(.2); backend.setMarginMm(.4); backend.undo();
            QCOMPARE(backend.marginMm(), .2);
        }
        {
            Appearance preferences(path);
            QVERIFY(preferences.dark());
            Backend backend(nullptr, &preferences);
            QCOMPARE(backend.marginMm(), .2);
            backend.clear(); QCOMPARE(backend.marginMm(), .2);
            backend.setMarginMm(.3);
        }
        Appearance restored(path);
        Backend nextSession(nullptr, &restored);
        QCOMPARE(nextSession.marginMm(), .3);
        QVERIFY(restored.dark());
        {
            QSettings legacy(path, QSettings::IniFormat);
            legacy.setValue("marginMm", .5); legacy.sync();
        }
        Appearance migrated(path);
        QCOMPARE(migrated.marginMm(), .5);
    }

    void exactTemplateAndBlankArea() {
        QImage source(600, 900, QImage::Format_RGB32); source.fill(Qt::blue);
        const auto sheet = Imaging::sheet(source, {}, Imaging::Dpi, 0);
        QCOMPARE(sheet.size(), QSize(2400, 3000));
        QVERIFY(qAbs(sheet.dotsPerMeterX() * .0254 - 300) < .02);
        double area = 0;
        const auto &layout = Imaging::layoutSlots();
        QCOMPARE(layout.size(), size_t(7));
        for (size_t i = 0; i < layout.size(); ++i) {
            const auto rect = layout[i].inches;
            QVERIFY(QRectF(0, 0, 8, 10).contains(rect));
            QCOMPARE(sheet.pixelColor(qRound(rect.center().x() * 300), qRound(rect.center().y() * 300)), QColor(Qt::blue));
            area += rect.width() * rect.height();
            for (size_t j = i + 1; j < layout.size(); ++j) QVERIFY(!rect.intersects(layout[j].inches));
        }
        QCOMPARE(area, 72.);
        QCOMPARE(sheet.pixelColor(2399, 0), QColor(Qt::white));
        QCOMPARE(sheet.pixelColor(1800, 1199), QColor(Qt::white));
        QCOMPARE(sheet.pixelColor(1800, 1200), QColor(Qt::blue));
        QCOMPARE(sheet.pixelColor(1199, 2999), QColor(Qt::blue));
        QCOMPARE(sheet.pixelColor(2399, 2999), QColor(Qt::blue));
    }

    void cropPreservesRatioAndClamps() {
        const auto center = Imaging::cropRect({1200, 1800}, {3, 2}, {});
        QCOMPARE(center, QRectF(0, 500, 1200, 800));
        QCOMPARE(Imaging::cropRect({1200, 1800}, {3, 2}, {0, 0, 1}), QRectF(0, 0, 1200, 800));
        QCOMPARE(Imaging::cropRect({1200, 1800}, {3, 2}, {1, 1, 2}), QRectF(600, 1400, 600, 400));
        const auto clamped = Imaging::cropRect({1200, 1800}, {4, 6}, {-99, 99, 99});
        QVERIFY(QRectF(0, 0, 1200, 1800).contains(clamped));
        QCOMPARE(clamped.width() / clamped.height(), 4. / 6.);
        QVERIFY(Imaging::resolutionWarning({1200, 1800}, {}).isEmpty());
        Imaging::Crops zoomed; zoomed[2].zoom = 2;
        QVERIFY(Imaging::resolutionWarning({1200, 1800}, zoomed).contains("150"));
    }

    void cutMarginUsesMillimetersAndPreservesPage() {
        QImage source(200, 300, QImage::Format_RGB32); source.fill(Qt::blue);
        for (double margin : {0., .1, .2, .3, .4, .5, 1., 2., 5., 10.}) {
            const auto sheet = Imaging::sheet(source, Imaging::DefaultCrops, Imaging::Dpi, margin);
            QCOMPARE(sheet.size(), QSize(2400, 3000));
            QVERIFY(qAbs(sheet.dotsPerMeterX() * .0254 - 300) < .02);
            const auto rect = Imaging::imageRect(QRectF(0, 0, 3, 2), margin);
            QVERIFY(qAbs(rect.width() * 25.4 - (76.2 - margin)) < 1e-8);
            QVERIFY(qAbs(rect.height() * 25.4 - (50.8 - margin)) < 1e-8);
            QCOMPARE(rect.topLeft(), QPointF(0, 0));
            const auto interior = Imaging::imageRect(QRectF(3, 2, 3, 2), margin);
            QVERIFY(qAbs(interior.width() * 25.4 - (76.2 - 2 * margin)) < 1e-8);
            QVERIFY(qAbs(interior.height() * 25.4 - (50.8 - 2 * margin)) < 1e-8);
            const auto bottomRight = Imaging::imageRect(QRectF(4, 4, 4, 6), margin);
            QCOMPARE(bottomRight.bottomRight(), QPointF(8, 10));
            int left = 900, right = 900;
            while (sheet.pixelColor(left - 1, 300) == QColor(Qt::white)) --left;
            while (sheet.pixelColor(right, 300) == QColor(Qt::white)) ++right;
            QVERIFY(qAbs((right - left) - 2 * margin * 300 / 25.4) <= 2);
            QCOMPARE(sheet.pixelColor(450, 300), QColor(Qt::blue));
            QCOMPARE(sheet.pixelColor(2399, 0), QColor(Qt::white));
            for (const QPoint &edge : {QPoint(0, 0), QPoint(450, 0), QPoint(1350, 0),
                 QPoint(0, 300), QPoint(0, 900), QPoint(0, 1650), QPoint(0, 2550),
                 QPoint(2399, 2100), QPoint(600, 2999), QPoint(1800, 2999), QPoint(2399, 2999)})
                QCOMPARE(sheet.pixelColor(edge), QColor(Qt::blue));
            if (margin > 0) {
                QCOMPARE(sheet.pixelColor(450, 599), QColor(Qt::white));
                QCOMPARE(sheet.pixelColor(1199, 1500), QColor(Qt::white));
            }
        }
        QVERIFY(Imaging::sheet(source, {}, 300, -.5).isNull());
        QVERIFY(Imaging::sheet(source, {}, 300, .05).isNull());
        QVERIFY(Imaging::sheet(source, {}, 300, .25).isNull());
        QVERIFY(Imaging::sheet(source, {}, 300, 10.5).isNull());
    }

    void marginAffectsEveryPhotoWithoutChangingIndividualRotations() {
        QTemporaryDir input, output;
        const auto a = imageFile(input.filePath("a.png"), Qt::blue);
        const auto b = imageFile(input.filePath("b.png"), Qt::red);
        Backend backend; backend.loadInitialPhotos({a, b});
        QCOMPARE(backend.marginMm(), .5);
        backend.setRotation(0, 90);
        backend.select(1); QCOMPARE(backend.current()->crops[0].rotation, 270);
        backend.setTargetFolder(QUrl::fromLocalFile(output.path()));
        backend.exportAll("png", true);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
        QVERIFY(!backend.dirty());
        backend.setMarginMm(2);
        QVERIFY(backend.dirty());
        QCOMPARE(backend.current()->status, QString("Pendiente"));
        QCOMPARE(backend.current()->crops[0].rotation, 270);
        backend.select(0); QCOMPARE(backend.current()->status, QString("Pendiente"));
        QCOMPARE(backend.current()->crops[0].rotation, 90);
        backend.undo(); QCOMPARE(backend.marginMm(), .5);
        backend.redo(); QCOMPARE(backend.marginMm(), 2.);
        backend.setMarginMm(.75); QCOMPARE(backend.marginMm(), 2.);
        backend.exportAll("png", true);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
        QVERIFY(!backend.dirty());
        for (const auto &name : {"a", "b"}) {
            const QImage first(output.filePath(QString("%1_8x10.png").arg(name)));
            const QImage second(output.filePath(QString("%1_8x10_2.png").arg(name)));
            QVERIFY(first.pixelColor(880, 300) != QColor(Qt::white));
            QCOMPARE(second.pixelColor(880, 300), QColor(Qt::white));
            QVERIFY(second.pixelColor(870, 300) != QColor(Qt::white));
            QVERIFY(second.pixelColor(0, 300) != QColor(Qt::white));
        }
        PreviewProvider provider(&backend);
        QSize size;
        const auto small = provider.requestImage("crop/0/0", &size, {});
        QCOMPARE(small.pixelColor(0, 100), QColor(Qt::blue));
        QCOMPARE(small.pixelColor(100, 0), QColor(Qt::blue));
        QCOMPARE(small.pixelColor(size.width() - 1, 100), QColor(Qt::white));
        QCOMPARE(small.pixelColor(100, size.height() - 1), QColor(Qt::white));
        const auto large = provider.requestImage("crop/2/0", &size, {});
        QCOMPARE(large.pixelColor(0, 200), QColor(Qt::white));
        QCOMPARE(large.pixelColor(200, 0), QColor(Qt::white));
        QCOMPARE(large.pixelColor(size.width() - 1, 200), QColor(Qt::blue));
        QCOMPARE(large.pixelColor(200, size.height() - 1), QColor(Qt::blue));
    }

    void appliesExifOrientation() {
        const auto path = fixture("rotated.jpg");
        QVERIFY(!path.isEmpty());
        QString error;
        const auto image = Imaging::load(path, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(image.size(), QSize(80, 120));
        QVERIFY(image.pixelColor(40, 10).red() > 180);
        QVERIFY(image.pixelColor(40, 110).blue() > 180);
        const auto result = scanPhotos({path}, false, flag());
        QCOMPARE(result.photos.size(), 1);
        QCOMPARE(result.photos.first().size, QSize(80, 120));
    }

    void rotationIsIndependentForEachPrintSize() {
        QImage source(200, 300, QImage::Format_RGB32);
        QPainter marker(&source);
        marker.fillRect(0, 0, 100, 150, Qt::red);
        marker.fillRect(100, 0, 100, 150, Qt::green);
        marker.fillRect(0, 150, 100, 150, Qt::blue);
        marker.fillRect(100, 150, 100, 150, Qt::yellow);
        marker.end();
        const std::array<std::array<QColor, 4>, 4> expected{{
            {Qt::red, Qt::green, Qt::blue, Qt::yellow},
            {Qt::blue, Qt::red, Qt::yellow, Qt::green},
            {Qt::yellow, Qt::blue, Qt::green, Qt::red},
            {Qt::green, Qt::yellow, Qt::red, Qt::blue}
        }};
        for (int turn = 0; turn < 4; ++turn) {
            const QSize size = Imaging::rotatedSize(source.size(), turn * 90);
            QImage painted(size, QImage::Format_RGB32); painted.fill(Qt::white);
            QPainter painter(&painted);
            Imaging::paintCrop(painter, QRectF(painted.rect()), source, {.5, .5, 1, turn * 90});
            painter.end();
            QCOMPARE(painted.pixelColor(10, 10), expected[turn][0]);
            QCOMPARE(painted.pixelColor(size.width() - 11, 10), expected[turn][1]);
            QCOMPARE(painted.pixelColor(10, size.height() - 11), expected[turn][2]);
            QCOMPARE(painted.pixelColor(size.width() - 11, size.height() - 11), expected[turn][3]);
        }
        Imaging::Crops crops;
        crops[0].rotation = 90; crops[1].rotation = 270;
        const auto sheet = Imaging::sheet(source, crops, 100);
        for (const auto &slot : Imaging::layoutSlots()) {
            const int turn = crops[slot.crop].rotation / 90;
            const auto &r = slot.inches;
            QCOMPARE(sheet.pixelColor(qRound((r.x() + r.width() * .25) * 100), qRound((r.y() + r.height() * .25) * 100)), expected[turn][0]);
            QCOMPARE(sheet.pixelColor(qRound((r.x() + r.width() * .75) * 100), qRound((r.y() + r.height() * .75) * 100)), expected[turn][3]);
        }
        QCOMPARE(sheet.pixelColor(799, 399), QColor(Qt::white));
        crops[2].rotation = 90;
        // The 4x6 crop has 300 ppi after a 90-degree turn,
        // rather than the 450 ppi available in its original vertical orientation.
        QVERIFY(Imaging::resolutionWarning({1800, 2700}, crops).isEmpty());
        crops[2].zoom = 2;
        QVERIFY(Imaging::resolutionWarning({1800, 2700}, crops).contains("150"));
    }

    void rotationSurvivesFramingAndBatchOperations() {
        QTemporaryDir dir;
        const auto a = imageFile(dir.filePath("a.png"));
        const auto b = imageFile(dir.filePath("b.png"));
        Backend backend; backend.loadInitialPhotos({a, b});
        QCOMPARE(backend.current()->crops[0].rotation, 270);
        QCOMPARE(backend.current()->crops[1].rotation, 270);
        QCOMPARE(backend.current()->crops[2].rotation, 0);
        backend.setRotation(0, 90); backend.setRotation(1, -90);
        QCOMPARE(backend.current()->crops[0].rotation, 90);
        QCOMPARE(backend.current()->crops[1].rotation, 270);
        QCOMPARE(backend.current()->crops[2].rotation, 0);
        backend.setCrop(0, .25, .75, 1.4);
        QCOMPARE(backend.current()->crops[0].rotation, 90);
        backend.undo(); QCOMPARE(backend.current()->crops[0].rotation, 90);
        backend.redo(); QCOMPARE(backend.current()->crops[0].zoom, 1.4);
        backend.applyToAll(); backend.select(1);
        QCOMPARE(backend.current()->crops[0].rotation, 90);
        QCOMPARE(backend.current()->crops[1].rotation, 270);
        backend.undo(); QCOMPARE(backend.current()->crops[0].rotation, 270);
        backend.redo(); QCOMPARE(backend.current()->crops[0].rotation, 90);
        const auto c = imageFile(dir.filePath("c.png"));
        backend.addUrls({QUrl::fromLocalFile(c)}, false);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        backend.select(2);
        QCOMPARE(backend.current()->crops[0].rotation, 90);
        QCOMPARE(backend.current()->crops[1].rotation, 270);
        backend.setRotation(0, 45); QCOMPARE(backend.current()->crops[0].rotation, 90);
        backend.resetCrops(); QCOMPARE(backend.current()->crops[0].rotation, 270);
        backend.undo(); QCOMPARE(backend.current()->crops[0].rotation, 90);
        backend.select(0); QCOMPARE(backend.current()->crops[0].rotation, 90);
        backend.clear();
        backend.addUrls({QUrl::fromLocalFile(a)}, false);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        QCOMPARE(backend.current()->crops[0].rotation, 270);
        QCOMPARE(backend.current()->crops[1].rotation, 270);
        QCOMPARE(backend.current()->crops[2].rotation, 0);
    }

    void scanRecursesDeduplicatesAndKeepsHierarchy() {
        QTemporaryDir dir;
        const auto a = imageFile(dir.filePath("Grupo A/alumno.png"));
        imageFile(dir.filePath("Grupo B/alumno.jpg"));
        imageFile(dir.filePath("solo.png"));
        QFile bad(dir.filePath("dañada.jpg")); QVERIFY(bad.open(QIODevice::WriteOnly)); bad.write("broken"); bad.close();
        QFile text(dir.filePath("notas.txt")); QVERIFY(text.open(QIODevice::WriteOnly)); text.write("ignore"); text.close();
        const auto flat = scanPhotos({dir.path()}, false, flag());
        QCOMPARE(flat.photos.size(), 1); QCOMPARE(flat.errors.size(), 1);
        const auto all = scanPhotos({dir.path(), a}, true, flag());
        QCOMPARE(all.photos.size(), 3); QCOMPARE(all.errors.size(), 1);
        QCOMPARE(all.photos.first().relative, QString("Grupo A/alumno.png"));
        auto cancelled = flag(); cancelled->store(true);
        QVERIFY(scanPhotos({dir.path()}, true, cancelled).cancelled);
    }

    void exportsWithoutOverwritingAndContinuesOnError() {
        QTemporaryDir input, output;
        const auto source = imageFile(input.filePath("alumno.png"));
        QFile original(source); QVERIFY(original.open(QIODevice::ReadOnly)); const auto before = original.readAll(); original.close();
        auto scan = scanPhotos({source}, false, flag());
        Photo broken = scan.photos.first(); broken.path = input.filePath("missing.jpg"); broken.relative = "Grupo/ausente.jpg";
        scan.photos.prepend(broken);
        auto first = exportPhotos(scan.photos, output.path(), "png", flag());
        QCOMPARE(first.completed, 1); QCOMPARE(first.failed, 1);
        auto second = exportPhotos(scan.photos.mid(1), output.path(), "png", flag());
        QCOMPARE(second.completed, 1);
        QVERIFY(QFile::exists(output.filePath("alumno_8x10.png")));
        QVERIFY(QFile::exists(output.filePath("alumno_8x10_2.png")));
        QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), before);
        const auto result = Imaging::load(output.filePath("alumno_8x10.png"), nullptr);
        QCOMPARE(result.size(), QSize(2400, 3000));
        QVERIFY(exportTarget(output.path(), "../escape.png", "png").isEmpty());
        QVERIFY(!containsPath(output.path(), output.path() + "-sibling"));
    }

    void portrait8x10KeepsOrientationAndFraming() {
        QImage source(200, 300, QImage::Format_RGB32);
        QPainter painter(&source);
        painter.fillRect(0, 0, 100, 150, Qt::red);
        painter.fillRect(100, 0, 100, 150, Qt::green);
        painter.fillRect(0, 150, 100, 150, Qt::blue);
        painter.fillRect(100, 150, 100, 150, Qt::yellow);
        painter.end();
        const std::array<std::array<QColor, 4>, 4> expected{{
            {Qt::red, Qt::green, Qt::blue, Qt::yellow},
            {Qt::blue, Qt::red, Qt::yellow, Qt::green},
            {Qt::yellow, Qt::blue, Qt::green, Qt::red},
            {Qt::green, Qt::yellow, Qt::red, Qt::blue}
        }};
        for (int turn = 0; turn < 4; ++turn) {
            const auto portrait = Imaging::portrait8x10(source, {.5, .5, 1, turn * 90}, 20);
            QCOMPARE(portrait.size(), QSize(160, 200));
            QCOMPARE(portrait.pixelColor(20, 20), expected[turn][0]);
            QCOMPARE(portrait.pixelColor(140, 20), expected[turn][1]);
            QCOMPARE(portrait.pixelColor(20, 180), expected[turn][2]);
            QCOMPARE(portrait.pixelColor(140, 180), expected[turn][3]);
        }
        const auto topLeft = Imaging::portrait8x10(source, {0, 0, 3, 0}, 20);
        const auto bottomRight = Imaging::portrait8x10(source, {1, 1, 3, 0}, 20);
        QCOMPARE(topLeft.pixelColor(150, 190), QColor(Qt::red));
        QCOMPARE(bottomRight.pixelColor(10, 10), QColor(Qt::yellow));

        // A square marker stays square when the source is cropped to fill 8x10.
        QImage square(400, 400, QImage::Format_RGB32); square.fill(Qt::white);
        QPainter marker(&square); marker.fillRect(150, 150, 100, 100, Qt::black); marker.end();
        const auto portrait = Imaging::portrait8x10(square, {}, 20);
        int width = 0, height = 0;
        for (int x = 0; x < portrait.width(); ++x) if (portrait.pixelColor(x, 100) == QColor(Qt::black)) ++width;
        for (int y = 0; y < portrait.height(); ++y) if (portrait.pixelColor(80, y) == QColor(Qt::black)) ++height;
        QCOMPARE(width, 50); QCOMPARE(height, 50);
    }

    void attachmentExportsAllFormats_data() {
        QTest::addColumn<QString>("format");
        for (const auto &format : {"jpg", "png", "pdf"}) QTest::newRow(format) << QString(format);
    }

    void attachmentExportsAllFormats() {
        QFETCH(QString, format);
        QTemporaryDir input, output;
        QDir().mkpath(input.filePath("Grupo"));
        const auto source = input.filePath("Grupo/alumna.jpg");
        QVERIFY(QFile::copy(fixture("rotated.jpg"), source));
        const auto photos = scanPhotos({input.path()}, true, flag()).photos;
        QCOMPARE(photos.size(), 1);
        const auto plain = exportPhotos(photos, output.path(), format, flag());
        QCOMPARE(plain.files, 1);
        const auto pair = exportPhotos(photos, output.path(), format, flag(), {}, .5, true);
        QCOMPARE(pair.completed, 1); QCOMPARE(pair.failed, 0); QCOMPARE(pair.files, 2);
        const auto attachment = output.filePath("Grupo/alumna_foto_8x10." + format);
        const auto sheet = output.filePath("Grupo/alumna_8x10." + format);
        const auto repeatedSheet = output.filePath("Grupo/alumna_8x10_2." + format);
        QVERIFY(QFileInfo(attachment).size() > 1000);
        QFile first(sheet), second(repeatedSheet);
        QVERIFY(first.open(QIODevice::ReadOnly)); QVERIFY(second.open(QIODevice::ReadOnly));
        // Adding the attachment leaves the existing package unchanged.
        if (format != "pdf") QCOMPARE(first.readAll(), second.readAll());
        if (format != "pdf") {
            const QImage portrait(attachment);
            QCOMPARE(portrait.size(), QSize(2400, 3000));
            QVERIFY(qAbs(portrait.dotsPerMeterX() * .0254 - 300) < 1);
            QVERIFY(qAbs(portrait.dotsPerMeterY() * .0254 - 300) < 1);
            QVERIFY(portrait.pixelColor(1200, 100).red() > 180);
            QVERIFY(portrait.pixelColor(1200, 2900).blue() > 180);
        } else {
            QFile pdf(attachment); QVERIFY(pdf.open(QIODevice::ReadOnly));
            const auto data = pdf.readAll(); QVERIFY(data.startsWith("%PDF-"));
            QVERIFY(data.contains("/MediaBox [0 0 576.000000 720.000000]") || data.contains("/MediaBox [0 0 576 720]"));
        }
        const auto repeated = exportPhotos(photos, output.path(), format, flag(), {}, .5, true);
        QCOMPARE(repeated.files, 2);
        QVERIFY(QFile::exists(output.filePath("Grupo/alumna_foto_8x10_2." + format)));
        QCOMPARE(QDir(output.filePath("Grupo")).entryList(QDir::Files).size(), 5);
    }

    void attachmentCancellationKeepsCompletePairs() {
        QTemporaryDir input, output;
        const auto a = imageFile(input.filePath("a.png"));
        const auto b = imageFile(input.filePath("b.png"));
        auto cancel = flag();
        const auto result = exportPhotos(scanPhotos({a, b}, false, flag()).photos, output.path(), "png", cancel,
            [&](int, const QString &status, const QString &, const QString &) { if (status == "Exportada") cancel->store(true); }, .5, true);
        QCOMPARE(result.completed, 1); QCOMPARE(result.files, 2); QVERIFY(result.cancelled);
        QVERIFY(QFile::exists(output.filePath("a_8x10.png")));
        QVERIFY(QFile::exists(output.filePath("a_foto_8x10.png")));
        QVERIFY(!QFile::exists(output.filePath("b_8x10.png")));
    }

    void attachmentMarksExportedPhotosPendingAndUsesPortraitCrop() {
        QTemporaryDir input, output;
        const auto source = input.filePath("alumna.jpg");
        QVERIFY(QFile::copy(fixture("rotated.jpg"), source));
        Backend backend; backend.loadInitialPhotos({source});
        QVERIFY(!backend.attach8x10());
        backend.setTargetFolder(QUrl::fromLocalFile(output.path()));
        backend.setRotation(2, 180); backend.setCrop(2, .2, .8, 1.3);
        const auto crops = backend.current()->crops;
        backend.exportAll("png", true);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
        QCOMPARE(backend.exportFiles(), 1); QVERIFY(!backend.dirty());
        backend.setAttach8x10(true); QVERIFY(backend.dirty());
        QVERIFY(backend.current()->crops == crops);
        backend.exportAll("png", true);
        backend.setAttach8x10(false); QVERIFY(backend.attach8x10()); // Locked during export.
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
        QCOMPARE(backend.exportFiles(), 2); QCOMPARE(backend.exportSucceeded(), 1);
        QVERIFY(!backend.dirty());
        const QImage portrait(output.filePath("alumna_foto_8x10.png"));
        const auto expected = Imaging::portrait8x10(Imaging::load(source, nullptr), crops[2]);
        QCOMPARE(portrait.convertToFormat(QImage::Format_RGB32), expected);
        backend.exportAll("png", true); QVERIFY(!backend.busy());
        QCOMPARE(QDir(output.path()).entryList(QDir::Files).size(), 3);
        backend.setAttach8x10(false); QVERIFY(backend.dirty());
        backend.exportAll("png", true);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
        QCOMPARE(backend.exportFiles(), 1);
        QCOMPARE(QDir(output.path()).entryList(QDir::Files).size(), 4);
    }

    void savesAllFormatsAndReportsWriteFailure() {
        QTemporaryDir dir;
        QImage source(60, 90, QImage::Format_RGB32); source.fill(Qt::red);
        auto sheet = Imaging::sheet(source, {});
        for (const auto &format : {"jpg", "png", "pdf"}) {
            QString error;
            const auto path = dir.filePath(QString("sheet.") + format);
            QVERIFY2(Imaging::save(sheet, path, format, &error), qPrintable(error));
            QVERIFY(QFileInfo(path).size() > 1000);
            if (QString(format) != "pdf") {
                const QImage loaded(path);
                QCOMPARE(loaded.size(), QSize(2400, 3000));
                QVERIFY(qAbs(loaded.dotsPerMeterX() * .0254 - 300) < 1);
            } else {
                QFile pdf(path); QVERIFY(pdf.open(QIODevice::ReadOnly)); const auto data = pdf.readAll();
                QVERIFY(data.startsWith("%PDF-")); QVERIFY(data.contains("/MediaBox [0 0 576.000000 720.000000]") || data.contains("/MediaBox [0 0 576 720]"));
            }
        }
        QString error;
        QVERIFY(!Imaging::save(sheet, dir.filePath("missing/out.png"), "png", &error));
        QVERIFY(!error.isEmpty()); QVERIFY(!QFile::exists(dir.filePath("missing/out.png")));
    }

    void cancellationKeepsFinishedFiles() {
        QTemporaryDir input, output;
        const auto a = imageFile(input.filePath("a.png"));
        const auto b = imageFile(input.filePath("b.png"));
        const auto photos = scanPhotos({a, b}, false, flag()).photos;
        auto cancel = flag();
        const auto result = exportPhotos(photos, output.path(), "jpg", cancel,
            [&](int, const QString &status, const QString &, const QString &) { if (status == "Exportada") cancel->store(true); });
        QCOMPARE(result.completed, 1); QVERIFY(result.cancelled);
        QVERIFY(QFile::exists(output.filePath("a_8x10.jpg")));
        QVERIFY(!QFile::exists(output.filePath("b_8x10.jpg")));
    }

    void batchOfFifty() {
        QTemporaryDir input, output;
        for (int i = 0; i < 50; ++i) imageFile(input.filePath(QString("Grupo/alumno_%1.jpg").arg(i, 3, 10, QChar('0'))));
        const auto scan = scanPhotos({input.path()}, true, flag());
        QCOMPARE(scan.photos.size(), 50);
        const auto result = exportPhotos(scan.photos, output.path(), "jpg", flag());
        QCOMPARE(result.completed, 50); QCOMPARE(result.failed, 0);
        QCOMPARE(QDir(output.filePath("Grupo")).entryList({"*.jpg"}, QDir::Files).size(), 50);
    }

    void backendCommonCropsUndoAndPendingResume() {
        QTemporaryDir input, output;
        const auto a = imageFile(input.filePath("a.png"));
        const auto b = imageFile(input.filePath("b.png"));
        Backend backend;
        backend.addUrls({QUrl::fromLocalFile(a), QUrl::fromLocalFile(b)}, true);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        QCOMPARE(backend.count(), 2);
        backend.setCrop(0, .2, .3, 1.5); backend.applyToAll();
        backend.select(1); QCOMPARE(backend.current()->crops[0].zoom, 1.5);
        backend.undo(); QCOMPARE(backend.current()->crops[0].zoom, 1.);
        backend.redo(); QCOMPARE(backend.current()->crops[0].zoom, 1.5);
        backend.setCrop(2, .7, .5, 1.2); backend.select(0);
        QCOMPARE(backend.current()->crops[2].zoom, 1.);
        backend.setTargetFolder(QUrl::fromLocalFile(output.path()));
        backend.exportAll("jpg", true);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
        QVERIFY(!backend.dirty());
        backend.exportAll("jpg", true);
        QVERIFY(!backend.busy());
        QCOMPARE(QDir(output.path()).entryList({"*.jpg"}, QDir::Files).size(), 2);
        backend.select(0); backend.setCrop(1, .5, .3, 1.1);
        backend.exportAll("jpg", true);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
        QCOMPARE(QDir(output.path()).entryList({"*.jpg"}, QDir::Files).size(), 3);
    }

    void backendRejectsDestinationInsideInputTree() {
        QTemporaryDir input;
        imageFile(input.filePath("a.png")); QDir().mkpath(input.filePath("salida"));
        Backend backend; backend.addUrls({QUrl::fromLocalFile(input.path())}, true);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        backend.setTargetFolder(QUrl::fromLocalFile(input.filePath("salida")));
        QVERIFY(backend.destination().isEmpty());
    }

    void canReaddSameImageAfterPdfExportAndRemoval() {
        QTemporaryDir dir;
        const auto original = imageFile(dir.filePath("alumna.png"));
        Backend backend;
        backend.addUrls({QUrl::fromLocalFile(original)}, false);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        QCOMPARE(backend.count(), 1);
        backend.setTargetFolder(QUrl::fromLocalFile(dir.path()));
        backend.exportAll("pdf", true);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
        QVERIFY(QFile::exists(dir.filePath("alumna_8x10.pdf")));
        backend.removeSelected(); QCOMPARE(backend.count(), 0);
        backend.addUrls({QUrl::fromLocalFile(original)}, false);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        QCOMPARE(backend.count(), 1);
        QCOMPARE(backend.current()->status, QString("Pendiente"));
        backend.exportAll("pdf", true);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
        QVERIFY(QFile::exists(dir.filePath("alumna_8x10_2.pdf")));
        backend.clear();
        backend.addUrls({QUrl::fromLocalFile(original)}, false);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        QCOMPARE(backend.count(), 1);
        backend.addUrls({QUrl::fromLocalFile(original)}, false);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        QCOMPARE(backend.count(), 1);
        QVERIFY(backend.message().contains("ya están en el lote"));
    }

    void remembersOutputFolderAcrossSessions() {
        QTemporaryDir dir;
        const QString output = dir.filePath(QString::fromUtf8("Salida fotos ñ"));
        QVERIFY(QDir().mkpath(output));
        const auto original = imageFile(QDir(output).filePath("alumna.png"));
        const auto settings = dir.filePath("preferences.ini");
        {
            Appearance preferences(settings);
            Backend backend(nullptr, &preferences);
            backend.setTargetFolder(QUrl::fromLocalFile(output));
            QCOMPARE(backend.destination(), output);
            backend.setMarginMm(3); preferences.setTheme("dark");
        }
        {
            Appearance preferences(settings);
            Backend backend(nullptr, &preferences);
            QCOMPARE(backend.destination(), output);
            QCOMPARE(backend.destinationUrl().toLocalFile(), output);
            QCOMPARE(backend.marginMm(), 3.); QVERIFY(preferences.dark());
            backend.addUrls({QUrl::fromLocalFile(original)}, false);
            QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
            QCOMPARE(backend.count(), 1);
            backend.exportAll("pdf", true);
            QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
            QVERIFY(QFile::exists(QDir(output).filePath("alumna_8x10.pdf")));
            backend.setTargetFolder(QUrl::fromLocalFile(original));
            QCOMPARE(backend.destination(), output);
        }
        QVERIFY(QDir(output).removeRecursively());
        Appearance preferences(settings);
        Backend missing(nullptr, &preferences);
        QVERIFY(missing.destination().isEmpty());
        QVERIFY(missing.message().contains(QString::fromUtf8("no está disponible")));
    }

    void folderScansExcludeOutputsAndExplainRejectedInputs() {
        QTemporaryDir dir;
        const auto original = imageFile(dir.filePath("alumna.png"));
        const QString output = dir.filePath("paquetes");
        const auto previous = imageFile(QDir(output).filePath("alumna_8x10.png"));
        const auto scan = scanPhotos({dir.path()}, true, flag(), Imaging::DefaultCrops, output);
        QCOMPARE(scan.photos.size(), 1); QCOMPARE(scan.photos.first().path, original);
        const auto explicitFile = scanPhotos({previous}, false, flag(), Imaging::DefaultCrops, output);
        QCOMPARE(explicitFile.photos.size(), 1);
        Backend backend; backend.setTargetFolder(QUrl::fromLocalFile(output));
        backend.addUrls({QUrl::fromLocalFile(output)}, true);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        QCOMPARE(backend.count(), 0); QVERIFY(backend.importError().contains("carpeta de salida"));
        const QString pdf = dir.filePath("documento.pdf");
        QFile file(pdf); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("%PDF-test"); file.close();
        backend.addUrls({QUrl::fromLocalFile(pdf)}, false);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        QVERIFY(backend.importError().contains("formato no compatible"));
        backend.addUrls({QUrl::fromLocalFile(original)}, false);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        QCOMPARE(backend.count(), 1); QVERIFY(backend.importError().isEmpty());
    }

    void removingLastFolderPhotoReleasesItsOutputRestriction() {
        QTemporaryDir dir;
        const auto original = imageFile(dir.filePath("alumna.png"));
        Backend backend; backend.addUrls({QUrl::fromLocalFile(dir.path())}, false);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        QCOMPARE(backend.count(), 1);
        backend.removeSelected();
        backend.addUrls({QUrl::fromLocalFile(original)}, false);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        backend.setTargetFolder(QUrl::fromLocalFile(dir.path()));
        QCOMPARE(backend.destination(), dir.path());
    }

    void exportSummaryReportsSuccessAndErrors_data() {
        QTest::addColumn<int>("missing");
        QTest::addColumn<QString>("outcome");
        QTest::newRow("success") << 0 << QString("success");
        QTest::newRow("partial") << 1 << QString("partial");
        QTest::newRow("error") << 2 << QString("error");
    }

    void exportSummaryReportsSuccessAndErrors() {
        QFETCH(int, missing); QFETCH(QString, outcome);
        QTemporaryDir input, output;
        const auto a = imageFile(input.filePath("a.png"));
        const auto b = imageFile(input.filePath("b.png"));
        Backend backend; backend.loadInitialPhotos({a, b});
        backend.setTargetFolder(QUrl::fromLocalFile(output.path()));
        if (missing >= 1) QVERIFY(QFile::remove(a));
        if (missing >= 2) QVERIFY(QFile::remove(b));
        QSignalSpy finished(&backend, &Backend::exportFinished);
        QVector<double> progress;
        connect(&backend, &Backend::changed, &backend, [&] { progress << backend.progress(); });
        backend.exportAll("png", true);
        QVERIFY(backend.exporting()); QCOMPARE(backend.exportTotal(), 2);
        const int selected = backend.selected(); const double margin = backend.marginMm();
        const auto crop = backend.current()->crops[0];
        backend.select(1); backend.setMarginMm(2); backend.setCrop(0, 0, 0, 2);
        backend.removeSelected(); backend.clear(); backend.addUrls({QUrl::fromLocalFile(a)}, false);
        QCOMPARE(backend.selected(), selected); QCOMPARE(backend.marginMm(), margin);
        QCOMPARE(backend.current()->crops[0].zoom, crop.zoom); QCOMPARE(backend.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.exporting(), 30000);
        QCOMPARE(finished.size(), 1); QCOMPARE(backend.exportOutcome(), outcome);
        QCOMPARE(backend.exportSucceeded(), 2 - missing); QCOMPARE(backend.exportFailed(), missing);
        QCOMPARE(backend.exportProcessed(), 2); QCOMPARE(backend.progress(), 1.);
        for (int i = 1; i < progress.size(); ++i) QVERIFY(progress[i] >= progress[i - 1]);
    }

    void exportProgressBlocksInterfaceAndCanCancel_data() {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("light");
        QTest::newRow("dark") << QString("dark");
    }

    void exportProgressBlocksInterfaceAndCanCancel() {
        QFETCH(QString, theme);
        QTemporaryDir input, output;
        QStringList paths;
        for (int i = 0; i < 50; ++i)
            paths << imageFile(input.filePath(QString("alumno_%1.png").arg(i, 3, 10, QChar('0'))));
        Appearance appearance(input.filePath("preferences.ini")); appearance.setTheme(theme);
        Backend backend(nullptr, &appearance); backend.loadInitialPhotos(paths);
        backend.setTargetFolder(QUrl::fromLocalFile(output.path()));
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QSignalSpy finished(&backend, &Backend::exportFinished);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("appearance", &appearance);
        engine.addImageProvider("preview", new PreviewProvider(&backend));
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        QTest::qWait(50);
        auto *workspace = findItem(window->contentItem(), "workspace"); QVERIFY(workspace);
        auto *progressDialog = window->findChild<QObject *>("exportProgressDialog"); QVERIFY(progressDialog);
        backend.exportAll("png", true);
        QVERIFY(backend.exporting()); QVERIFY(!workspace->isEnabled());
        QVERIFY(progressDialog->property("visible").toBool());
        auto *spinner = findItem(window->contentItem(), "exportSpinner"); QVERIFY(spinner);
        QVERIFY(spinner->property("running").toBool());
        auto *add = findItem(window->contentItem(), "addImages"); QVERIFY(add);
        QVERIFY(!add->isEnabled());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(add));
        QTest::keyClick(window, Qt::Key_O, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_Question);
        QTest::keyClick(window, Qt::Key_Q);
        QTest::keyClick(window, Qt::Key_Escape);
        QVERIFY(!window->close());
        QVERIFY(window->isVisible()); QVERIFY(progressDialog->property("visible").toBool());
        QVERIFY(!window->findChild<QObject *>("imagesDialog")->property("visible").toBool());
        QVERIFY(!window->findChild<QObject *>("helpDialog")->property("visible").toBool());
        auto *themePicker = findItem(window->contentItem(), "themePicker"); QVERIFY(themePicker);
        QVERIFY(!themePicker->isEnabled()); QCOMPARE(appearance.theme(), theme);
        QTRY_VERIFY_WITH_TIMEOUT(backend.exportProcessed() > 0, 10000);
        QVERIFY(backend.progress() > 0 && backend.progress() < 1);
        auto *bar = findItem(window->contentItem(), "exportProgressBar"); QVERIFY(bar);
        QCOMPARE(bar->property("value").toDouble(), backend.progress());
        const QString captureDirectory = qEnvironmentVariable("PHOTO_CONVERTER_CAPTURE_DIR");
        if (!captureDirectory.isEmpty())
            QVERIFY(window->grabWindow().save(QDir(captureDirectory).filePath("exportando-" + theme + ".png")));
        auto *cancel = findItem(window->contentItem(), "cancelExportButton"); QVERIFY(cancel);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(cancel));
        QVERIFY(backend.cancelRequested()); QVERIFY(!cancel->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!backend.exporting(), 30000);
        QCOMPARE(finished.size(), 1); QCOMPARE(backend.exportOutcome(), QString("cancelled"));
        QVERIFY(backend.exportSucceeded() > 0 && backend.exportSucceeded() < backend.exportTotal());
        QCOMPARE(QDir(output.path()).entryList({"*.png"}, QDir::Files).size(), backend.exportSucceeded());
        auto *result = window->findChild<QObject *>("exportResultDialog"); QVERIFY(result);
        QTRY_VERIFY(result->property("visible").toBool());
        QVERIFY(!progressDialog->property("visible").toBool()); QVERIFY(workspace->isEnabled());
        if (!captureDirectory.isEmpty())
            QVERIFY(window->grabWindow().save(QDir(captureDirectory).filePath("exportacion-cancelada-" + theme + ".png")));
        auto *accept = findItem(window->contentItem(), "dismissExportResult"); QVERIFY(accept);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(accept));
        QTRY_VERIFY(!result->property("visible").toBool());
        backend.select(1); QCOMPARE(backend.selected(), 1);
        QCOMPARE(warnings.size(), 0);
        window->setProperty("allowClose", true); window->close();
    }

    void realInterfaceRotationSliderUndoAndExport_data() {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("light");
        QTest::newRow("dark") << QString("dark");
    }

    void realInterfaceRotationSliderUndoAndExport() {
        QFETCH(QString, theme);
        QTemporaryDir input, output;
        const auto a = imageFile(input.filePath("alumno.png"), Qt::blue, {1200, 1800});
        const QImage demo(QFINDTESTDATA("../docs/demo-portrait.png"));
        QVERIFY(!demo.isNull()); QVERIFY(demo.copy(300, 0, 1200, 1800).save(a));
        Appearance appearance(input.filePath("appearance.ini")); appearance.setTheme(theme);
        Backend backend(nullptr, &appearance); backend.loadInitialPhotos({a});
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("appearance", &appearance);
        engine.addImageProvider("preview", new PreviewProvider(&backend));
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window); QTest::qWait(100);
        auto *palette = window->findChild<QObject *>("themePalette");
        QVERIFY(palette);
        const std::array<std::pair<const char *, const char *>, 15> textPairs{{
            {"text", "surface"}, {"text", "panel"}, {"text", "field"},
            {"secondaryText", "surface"}, {"secondaryText", "canvas"}, {"secondaryText", "selected"},
            {"disabledText", "disabledSurface"}, {"accentText", "accent"}, {"accentText", "accentHover"},
            {"warningText", "warningSurface"}, {"error", "errorSurface"}, {"success", "panel"},
            {"paperText", "paperTag"}, {"paperMuted", "paper"}, {"text", "hover"}
        }};
        for (const auto &pair : textPairs) {
            const double ratio = contrast(palette->property(pair.first).value<QColor>(), palette->property(pair.second).value<QColor>());
            QVERIFY2(ratio >= 4.5, qPrintable(QString("%1 / %2: %3").arg(pair.first, pair.second).arg(ratio)));
        }
        for (const auto &background : {"surface", "hover", "selected"})
            QVERIFY(contrast(palette->property("border").value<QColor>(), palette->property(background).value<QColor>()) >= 3);
        QVERIFY(contrast(palette->property("track").value<QColor>(), palette->property("surface").value<QColor>()) >= 3);
        for (const auto &name : {"recursiveCheck", "guidesCheck", "pendingOnly"}) {
            auto *check = findItem(window->contentItem(), name); QVERIFY(check);
            auto *text = check->property("contentItem").value<QQuickItem *>(); QVERIFY(text);
            QCOMPARE(text->property("color").value<QColor>(), palette->property("text").value<QColor>());
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(check));
            QVERIFY(!check->property("checked").toBool());
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(check));
            QVERIFY(check->property("checked").toBool());
        }
        auto *paper = findItem(window->contentItem(), "paper"); QVERIFY(paper);
        auto *attachment = findItem(window->contentItem(), "attach8x10Check"); QVERIFY(attachment);
        QVERIFY(!attachment->property("checked").toBool());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(attachment));
        QVERIFY(backend.attach8x10());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(attachment));
        QVERIFY(!backend.attach8x10());
        QCOMPARE(paper->property("color").value<QColor>(), QColor(Qt::white));
        auto *margin = findItem(window->contentItem(), "marginBox");
        QVERIFY(margin); QCOMPARE(backend.marginMm(), .5);
        auto *editor = findItem(window->contentItem(), "marginEditor"); QVERIFY(editor);
        QCOMPARE(editor->property("color").value<QColor>(), palette->property("text").value<QColor>());
        const QString captureDirectory = qEnvironmentVariable("PHOTO_CONVERTER_CAPTURE_DIR");
        if (!captureDirectory.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(window->grabWindow().save(QDir(captureDirectory).filePath(theme == "light" ? "tema-claro.png" : "tema-oscuro.png")));
        }
        const QString capture = qEnvironmentVariable("PHOTO_CONVERTER_CAPTURE");
        if (!capture.isEmpty() && theme == "light") { QTest::qWait(100); QVERIFY(window->grabWindow().save(capture)); }
        auto *themePicker = findItem(window->contentItem(), "themePicker"); QVERIFY(themePicker);
        const QImage beforeThemeChange = Imaging::sheet(QImage(a), backend.current()->crops, Imaging::Dpi, backend.marginMm());
        themePicker->forceActiveFocus();
        QTest::keyClick(window, theme == "light" ? Qt::Key_Down : Qt::Key_Up);
        QCOMPARE(appearance.theme(), theme == "light" ? QString("dark") : QString("light"));
        QCOMPARE(editor->property("color").value<QColor>(), palette->property("text").value<QColor>());
        QCOMPARE(paper->property("color").value<QColor>(), QColor(Qt::white));
        QCOMPARE(Imaging::sheet(QImage(a), backend.current()->crops, Imaging::Dpi, backend.marginMm()), beforeThemeChange);
        appearance.setTheme(theme);
        margin->forceActiveFocus(); QTest::keyClick(window, Qt::Key_Up);
        QCOMPARE(backend.marginMm(), .6);
        QTest::keyClick(window, Qt::Key_Down);
        QCOMPARE(backend.marginMm(), .5);
        auto *rotation = findItem(window->contentItem(), "rotationBox");
        QVERIFY(rotation);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(rotation));
        auto *popup = rotation->property("popup").value<QObject *>(); QVERIFY(popup);
        QTRY_VERIFY(popup->property("visible").toBool());
        if (!captureDirectory.isEmpty()) {
            QTest::qWait(150);
            QVERIFY(window->grabWindow().save(QDir(captureDirectory).filePath("selector-" + theme + ".png")));
        }
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!popup->property("visible").toBool());
        auto *help = window->findChild<QObject *>("helpDialog"); QVERIFY(help);
        QVERIFY(QMetaObject::invokeMethod(help, "open"));
        QTRY_VERIFY(help->property("visible").toBool());
        auto *helpHeader = help->property("header").value<QQuickItem *>(); QVERIFY(helpHeader);
        QCOMPARE(helpHeader->property("color").value<QColor>(), palette->property("text").value<QColor>());
        if (!captureDirectory.isEmpty()) {
            QTest::qWait(150);
            QVERIFY(window->grabWindow().save(QDir(captureDirectory).filePath("ayuda-" + theme + ".png")));
        }
        QVERIFY(QMetaObject::invokeMethod(help, "close"));
        QTRY_VERIFY(!help->property("visible").toBool());
        rotation->forceActiveFocus();
        QCOMPARE(rotation->property("currentIndex").toInt(), 3);
        QCOMPARE(backend.current()->crops[1].rotation, 270);
        QTest::keyClick(window, Qt::Key_Up);
        QCOMPARE(backend.current()->crops[0].rotation, 180);
        auto *middleRatio = findItem(window->contentItem(), "ratio_1");
        QVERIFY(middleRatio); QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(middleRatio));
        QCOMPARE(rotation->property("currentIndex").toInt(), 3);
        rotation->forceActiveFocus(); QTest::keyClick(window, Qt::Key_Up);
        QCOMPARE(backend.current()->crops[1].rotation, 180);
        auto *ratio = findItem(window->contentItem(), "ratio_2");
        QVERIFY(ratio); QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(ratio));
        QCOMPARE(window->property("cropIndex").toInt(), 2);
        QCOMPARE(rotation->property("currentIndex").toInt(), 0);
        QCOMPARE(backend.current()->crops[2].rotation, 0);
        auto *slider = findItem(window->contentItem(), "zoomSlider");
        QVERIFY(slider);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(slider, {slider->width() * .55, slider->height() / 2}));
        QVERIFY(backend.current()->crops[2].zoom > 1.4);
        QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(backend.current()->crops[2].zoom, 1.);
        QCOMPARE(backend.current()->crops[0].rotation, 180);
        QCOMPARE(backend.current()->crops[1].rotation, 180);
        margin->forceActiveFocus(); QTest::keyClick(window, Qt::Key_Up);
        QCOMPARE(backend.marginMm(), .6);
        backend.setTargetFolder(QUrl::fromLocalFile(output.path()));
        auto *folderDialog = window->findChild<QObject *>("targetFolderDialog"); QVERIFY(folderDialog);
        QCOMPARE(folderDialog->property("currentFolder").toUrl().toLocalFile(), output.path());
        auto *button = findItem(window->contentItem(), "exportButton");
        QVERIFY(button); QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(button));
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
        QVERIFY(QFile::exists(output.filePath("alumno_8x10.jpg")));
        auto *resultDialog = window->findChild<QObject *>("exportResultDialog"); QVERIFY(resultDialog);
        QTRY_VERIFY(resultDialog->property("visible").toBool());
        QCOMPARE(backend.exportOutcome(), QString("success"));
        QCOMPARE(backend.exportSucceeded(), 1); QCOMPARE(backend.exportFailed(), 0);
        if (!captureDirectory.isEmpty())
            QVERIFY(window->grabWindow().save(QDir(captureDirectory).filePath("exportacion-completada-" + theme + ".png")));
        auto *acceptResult = findItem(window->contentItem(), "dismissExportResult"); QVERIFY(acceptResult);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(acceptResult));
        QTRY_VERIFY(!resultDialog->property("visible").toBool());
        const QImage exported(output.filePath("alumno_8x10.jpg"));
        QVERIFY(exported.pixelColor(0, 300) != QColor(Qt::white));
        const auto separator = exported.pixelColor(900, 300);
        QVERIFY(separator.red() >= 248 && separator.green() >= 248 && separator.blue() >= 248);
        const QImage expectedSheet = Imaging::sheet(QImage(a), backend.current()->crops, Imaging::Dpi, backend.marginMm());
        // Compare well inside each print to allow normal JPEG compression noise.
        for (const auto &slot : Imaging::layoutSlots()) {
            const QPoint p(qRound(slot.inches.center().x() * 300), qRound(slot.inches.center().y() * 300));
            const auto actual = exported.pixelColor(p), expectedColor = expectedSheet.pixelColor(p);
            QVERIFY(qAbs(actual.red() - expectedColor.red()) < 12);
            QVERIFY(qAbs(actual.green() - expectedColor.green()) < 12);
            QVERIFY(qAbs(actual.blue() - expectedColor.blue()) < 12);
        }
        window->setWidth(1080); window->setHeight(740); QTest::qWait(80);
        QVERIFY(!window->grabWindow().isNull());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(attachment));
        QVERIFY(backend.attach8x10()); QVERIFY(backend.dirty());
        QVERIFY(point(attachment).x() < point(button).x());
        if (!captureDirectory.isEmpty())
            QVERIFY(window->grabWindow().save(QDir(captureDirectory).filePath("adjuntar-8x10-" + theme + ".png")));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(button));
        QVERIFY(!attachment->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 30000);
        QTRY_VERIFY(resultDialog->property("visible").toBool());
        QCOMPARE(backend.exportFiles(), 2); QCOMPARE(backend.exportSucceeded(), 1);
        QVERIFY(QFile::exists(output.filePath("alumno_8x10_2.jpg")));
        const QImage attached(output.filePath("alumno_foto_8x10.jpg"));
        QCOMPARE(attached.size(), QSize(2400, 3000));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point(acceptResult));
        QTRY_VERIFY(!resultDialog->property("visible").toBool());
        backend.clear(); QTest::qWait(100);
        auto *rotationText = rotation->property("contentItem").value<QQuickItem *>(); QVERIFY(rotationText);
        QCOMPARE(rotationText->property("color").value<QColor>(), palette->property("disabledText").value<QColor>());
        if (!captureDirectory.isEmpty())
            QVERIFY(window->grabWindow().save(QDir(captureDirectory).filePath("sin-fotos-" + theme + ".png")));
        backend.addUrls({QUrl::fromLocalFile(input.filePath("missing.png"))}, false);
        QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 5000);
        QVERIFY(!backend.importError().isEmpty());
        auto *errorBanner = findItem(window->contentItem(), "importErrorBanner"); QVERIFY(errorBanner);
        QVERIFY(errorBanner->isVisible());
        QCOMPARE(errorBanner->property("color").value<QColor>(), palette->property("errorSurface").value<QColor>());
        if (!captureDirectory.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(window->grabWindow().save(QDir(captureDirectory).filePath("error-carga-" + theme + ".png")));
        }
        QCOMPARE(warnings.size(), 0);
        window->setProperty("allowClose", true); window->close();
    }
};
QTEST_MAIN(PhotoConverterTests)
#include "photo-converter_tests.moc"

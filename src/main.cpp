#include "backend.h"
#include "appearance.h"
#include "previewprovider.h"
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>

static int cli(const QStringList &arguments) {
    const QString command = arguments.value(1);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Photo Converter · hojas de graduación de 8 × 10 pulgadas a 300 ppp."));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("command", "open, check, export, skill");
    parser.addPositionalArgument("inputs", "Imágenes JPG/PNG o carpetas", "[inputs...]");
    parser.addOption({{"o", "output"}, "Carpeta de salida", "directory"});
    parser.addOption({{"f", "format"}, "jpg, png o pdf", "format", "jpg"});
    parser.addOption({"recursive", "Incluir subcarpetas"});
    parser.addOption({"json", "Respuesta JSON"});
    parser.addOption({"margin-mm", "Margen solo en líneas interiores, por lado, de 0 a 10 mm en pasos de 0.1 mm", "mm", "0.5"});
    parser.process(arguments);
    auto fail = [&](const QString &message) {
        QTextStream(stderr) << message << '\n';
        if (parser.isSet("json")) QTextStream(stdout) << QJsonDocument(QJsonObject{{"error", message}}).toJson(QJsonDocument::Compact) << '\n';
        return 1;
    };
    QStringList inputs = parser.positionalArguments();
    if (!inputs.isEmpty()) inputs.removeFirst();
    if (command == "skill") {
        if (inputs != QStringList{"install"} || parser.value("output").isEmpty())
            return fail("Uso: photo-converter skill install --output CARPETA [--json]");
        const QString folder = QDir(parser.value("output")).filePath("photo-converter");
        if (!QDir().mkpath(folder)) return fail("No se pudo crear la carpeta de la habilidad.");
        const auto target = QDir(folder).filePath("SKILL.md");
        if (QFile::exists(target)) return fail("SKILL.md ya existe; no se reemplazó.");
        QFile source(":/agent-skill.md");
        QSaveFile file(target);
        if (!source.open(QIODevice::ReadOnly) || !file.open(QIODevice::WriteOnly)) return fail("No se pudo guardar la habilidad.");
        const auto data = source.readAll();
        if (file.write(data) != data.size() || !file.commit()) return fail("No se pudo guardar la habilidad.");
        QTextStream(stdout) << (parser.isSet("json") ? QString::fromUtf8(QJsonDocument(QJsonObject{{"path", target}}).toJson(QJsonDocument::Compact)) : target) << '\n';
        return 0;
    }
    if (command != "check" && command != "export") return fail("Comando no reconocido. Usa open, check, export o --help.");
    if (inputs.isEmpty()) return fail("Indica al menos una imagen o carpeta.");
    const QString output = parser.value("output"), format = parser.value("format").toLower();
    bool marginOk = false;
    const double marginMm = parser.value("margin-mm").toDouble(&marginOk);
    if (!marginOk || !Imaging::validMargin(marginMm)) return fail("Margen no válido. Usa --margin-mm de 0 a 10 en pasos de 0.1 mm.");
    if (command == "export") {
        if (output.isEmpty()) return fail("Indica --output CARPETA.");
        if (!QStringList{"jpg", "png", "pdf"}.contains(format)) return fail("Formato no compatible. Usa jpg, png o pdf.");
        for (const auto &input : inputs)
            if (QFileInfo(input).isDir() && containsPath(input, output)) return fail("La salida debe estar fuera de las carpetas de origen.");
    }
    const auto cancel = std::make_shared<std::atomic_bool>(false);
    const auto scan = scanPhotos(inputs, parser.isSet("recursive"), cancel);
    QJsonArray errors, photos;
    for (const auto &error : scan.errors) { errors.append(error); QTextStream(stderr) << error << '\n'; }
    if (scan.photos.isEmpty()) return fail("No se encontraron fotografías JPG o PNG válidas.");
    if (command == "check") {
        int failed = scan.errors.size();
        for (const auto &photo : scan.photos) {
            QString error;
            Imaging::load(photo.path, &error, 240);
            if (!error.isEmpty()) { ++failed; QTextStream(stderr) << photo.relative << ": " << error << '\n'; }
            photos.append(QJsonObject{{"input", photo.path}, {"relative", photo.relative}, {"width", photo.size.width()},
                {"height", photo.size.height()}, {"warning", Imaging::resolutionWarning(photo.size, photo.crops, marginMm)}, {"error", error}});
        }
        QJsonObject result{{"photos", photos}, {"errors", errors}, {"count", scan.photos.size()}, {"failed", failed}};
        QTextStream(stdout) << (parser.isSet("json") ? QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))
            : QStringLiteral("%1 fotografías · %2 errores").arg(scan.photos.size()).arg(failed)) << '\n';
        return failed ? 1 : 0;
    }
    const auto result = exportPhotos(scan.photos, output, format, cancel,
        [&](int index, const QString &status, const QString &target, const QString &error) {
            if (status == QStringLiteral("Procesando")) return;
            photos.append(QJsonObject{{"input", scan.photos.at(index).path}, {"output", target}, {"error", error}});
            if (!error.isEmpty()) QTextStream(stderr) << scan.photos.at(index).relative << ": " << error << '\n';
        }, marginMm);
    auto json = result.toJson(); json.insert("photos", photos); json.insert("errors", errors); json.insert("margin_mm_per_side", marginMm);
    QTextStream(stdout) << (parser.isSet("json") ? QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact))
        : QStringLiteral("%1 exportadas · %2 errores").arg(result.completed).arg(result.failed + scan.errors.size())) << '\n';
    return result.failed || !scan.errors.isEmpty() ? 1 : 0;
}

int main(int argc, char *argv[]) {
    const QString first = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    const bool headless = first == "export" || first == "check" || first == "skill" || first == "--help" || first == "--version";
    if (headless) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("QT_QPA_PLATFORMTHEME", "generic");
    }
    QGuiApplication app(argc, argv);
    app.setApplicationName("Photo Converter");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("PhotoConverter");
    app.setDesktopFileName("photo-converter");
    app.setWindowIcon(QIcon(":/icon.png"));
    if (headless) return cli(app.arguments());
    QQuickStyle::setStyle("Basic");
    Appearance appearance(qEnvironmentVariable("PHOTO_CONVERTER_SETTINGS_FILE"));
    Backend backend(nullptr, &appearance);
    QStringList inputs = app.arguments().mid(1);
    if (inputs.value(0) == "open") inputs.removeFirst();
    const int themeIndex = inputs.indexOf("--theme");
    if (themeIndex >= 0) {
        const QString theme = inputs.value(themeIndex + 1);
        if (theme != "light" && theme != "dark") { QTextStream(stderr) << "Usa --theme light o --theme dark.\n"; return 1; }
        appearance.setTheme(theme);
        inputs.removeAt(themeIndex + 1); inputs.removeAt(themeIndex);
    }
    QString screenshot;
    const int captureIndex = inputs.indexOf("--capture");
    if (captureIndex >= 0 && captureIndex + 1 < inputs.size()) {
        screenshot = inputs.at(captureIndex + 1);
        inputs.removeAt(captureIndex + 1); inputs.removeAt(captureIndex);
    }
    if (!inputs.isEmpty()) backend.loadInitialPhotos(inputs);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("appearance", &appearance);
    engine.addImageProvider("preview", new PreviewProvider(&backend));
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    if (!screenshot.isEmpty()) {
        QTimer::singleShot(1800, &app, [&] {
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            const bool ok = window && window->grabWindow().save(screenshot);
            app.exit(ok ? 0 : 1);
        });
    }
    return app.exec();
}

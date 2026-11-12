#include "batch.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QSet>
#include <algorithm>

QString canonicalPath(const QString &path) {
    const QFileInfo info(path);
    QString result = info.canonicalFilePath();
    if (result.isEmpty()) result = info.absoluteFilePath();
    result = QDir::cleanPath(QDir::fromNativeSeparators(result));
#ifdef Q_OS_WIN
    result = result.toCaseFolded();
#endif
    return result;
}

bool containsPath(const QString &parent, const QString &child) {
    const QString p = canonicalPath(parent), c = canonicalPath(child);
    return c == p || c.startsWith(p.endsWith('/') ? p : p + '/');
}

ScanResult scanPhotos(const QStringList &paths, bool recursive, const CancelFlag &cancel,
                      const Imaging::Crops &defaults, const QString &excludedDirectory) {
    ScanResult result;
    QSet<QString> seen;
    auto add = [&](const QString &path, const QString &relative, bool explicitFile) {
        if (cancel->load()) { result.cancelled = true; return; }
        const auto key = canonicalPath(path);
        if (seen.contains(key)) return;
        seen.insert(key);
        const QFileInfo info(path);
        const QString extension = info.suffix().toLower();
        if (!QStringList{"jpg", "jpeg", "png"}.contains(extension)) {
            if (explicitFile) result.errors << QStringLiteral("%1: formato no compatible. Selecciona una imagen JPG o PNG.").arg(info.fileName());
            return;
        }
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QSize size = reader.size();
        if (reader.transformation() & QImageIOHandler::TransformationRotate90) size.transpose();
        if (!reader.canRead() || !size.isValid()) {
            result.errors << QStringLiteral("%1: imagen no válida (%2)").arg(info.fileName(), reader.errorString());
            return;
        }
        Photo photo;
        photo.path = info.absoluteFilePath();
        photo.relative = QDir::fromNativeSeparators(relative);
        photo.size = size;
        photo.crops = defaults;
        result.photos.append(photo);
    };
    for (const QString &path : paths) {
        if (cancel->load()) { result.cancelled = true; break; }
        const QFileInfo info(path);
        if (!info.exists()) { result.errors << QStringLiteral("No existe: %1").arg(path); continue; }
        if (info.isDir()) {
            if (!excludedDirectory.isEmpty() && containsPath(excludedDirectory, info.absoluteFilePath())) {
                result.errors << QStringLiteral("%1: es la carpeta de salida o una de sus subcarpetas. Para cargar un original que esté ahí, selecciónalo con Agregar fotos.").arg(info.absoluteFilePath());
                continue;
            }
            QDir root(info.absoluteFilePath());
            QDirIterator it(root.absolutePath(), QDir::Files | QDir::Readable | QDir::NoSymLinks,
                            recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
            while (it.hasNext() && !cancel->load()) {
                const auto file = it.next();
                // Folder imports skip exports; an explicitly selected image remains valid anywhere.
                if (!excludedDirectory.isEmpty() && containsPath(excludedDirectory, file)) continue;
                add(file, root.relativeFilePath(file), false);
            }
            if (std::any_of(result.photos.begin(), result.photos.end(), [&](const Photo &photo) {
                return containsPath(root.absolutePath(), photo.path);
            })) result.roots << root.absolutePath();
        } else add(path, info.fileName(), true);
    }
    result.cancelled = result.cancelled || cancel->load();
    result.roots.removeDuplicates();
    std::sort(result.photos.begin(), result.photos.end(), [](const Photo &a, const Photo &b) {
        const int relative = QString::compare(a.relative, b.relative, Qt::CaseInsensitive);
        return relative == 0 ? a.path < b.path : relative < 0;
    });
    return result;
}

QString exportTarget(const QString &directory, const QString &relative, const QString &format, bool portrait) {
    // Only relative paths from our scanner are accepted, even for CLI callers.
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(relative));
    if (QDir::isAbsolutePath(clean) || clean == ".." || clean.startsWith("../")) return {};
    const QFileInfo info(clean);
    const QString parent = info.path() == "." ? directory : QDir(directory).filePath(info.path());
    if (!QDir().mkpath(parent)) return {};
    if (!containsPath(directory, parent)) return {};
    const QString stem = info.completeBaseName() + (portrait ? QStringLiteral("_foto_8x10") : QStringLiteral("_8x10"));
    QString target = QDir(parent).filePath(stem + '.' + format);
    int suffix = 2;
    while (QFileInfo::exists(target))
        target = QDir(parent).filePath(QStringLiteral("%1_%2.%3").arg(stem).arg(suffix++).arg(format));
    return target;
}

ExportResult exportPhotos(const QVector<Photo> &photos, const QString &directory,
                          const QString &format, const CancelFlag &cancel,
                          const std::function<void(int, const QString &, const QString &, const QString &)> &progress,
                          double marginMm, bool attach8x10) {
    ExportResult result;
    if (!QStringList{"jpg", "png", "pdf"}.contains(format) || !Imaging::validMargin(marginMm)) { result.failed = photos.size(); return result; }
    for (int index = 0; index < photos.size(); ++index) {
        if (cancel->load()) { result.cancelled = true; break; }
        const auto &photo = photos.at(index);
        if (progress) progress(index, QStringLiteral("Procesando"), {}, {});
        QString error, output;
        const QImage source = Imaging::load(photo.path, &error);
        if (!source.isNull()) {
            if (cancel->load()) { result.cancelled = true; break; }
            output = exportTarget(directory, photo.relative, format);
            if (output.isEmpty()) error = QStringLiteral("No se pudo crear una ruta de salida segura.");
            else if (canonicalPath(output) == canonicalPath(photo.path)) error = QStringLiteral("El destino coincide con el original.");
            else {
                const auto sheet = Imaging::sheet(source, photo.crops, Imaging::Dpi, marginMm);
                if (cancel->load()) { result.cancelled = true; break; }
                if (Imaging::save(sheet, output, format, &error)) {
                    ++result.files;
                    // Finish both files for this photo before honouring cancellation.
                    // Only a complete pair is marked exported for pending-only resumes.
                    if (attach8x10) {
                        const QString portraitOutput = exportTarget(directory, photo.relative, format, true);
                        if (portraitOutput.isEmpty()) error = QStringLiteral("Adjunto 8x10: no se pudo crear una ruta de salida segura.");
                        else if (canonicalPath(portraitOutput) == canonicalPath(photo.path)) error = QStringLiteral("Adjunto 8x10: el destino coincide con el original.");
                        else if (Imaging::save(Imaging::portrait8x10(source, photo.crops[2]), portraitOutput, format, &error)) ++result.files;
                        else error = QStringLiteral("Adjunto 8x10: %1").arg(error);
                    }
                }
            }
        }
        if (error.isEmpty()) ++result.completed; else ++result.failed;
        if (progress) progress(index, error.isEmpty() ? QStringLiteral("Exportada") : QStringLiteral("Error"), output, error);
    }
    return result;
}

QJsonObject ExportResult::toJson() const {
    return {{"completed", completed}, {"failed", failed}, {"cancelled", cancelled}};
}

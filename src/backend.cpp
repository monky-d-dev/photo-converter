#include "backend.h"
#include "appearance.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

Backend::Backend(QObject *parent, Appearance *preferences) : QAbstractListModel(parent), m_preferences(preferences) {
    if (m_preferences) {
        m_marginMm = m_preferences->marginMm();
        const QString saved = m_preferences->destination();
        if (!saved.isEmpty()) {
            if (QFileInfo(saved).isDir()) m_destination = QDir::cleanPath(QFileInfo(saved).absoluteFilePath());
            else m_message = QStringLiteral("La última carpeta de salida no está disponible. Selecciona otra carpeta: %1").arg(saved);
        }
    }
    connect(&m_scan, &QFutureWatcher<ScanResult>::finished, this, [this] {
        const auto result = m_scan.result();
        QSet<QString> existing;
        for (const auto &p : m_photos) existing.insert(canonicalPath(p.path));
        int added = 0, duplicates = 0;
        beginResetModel();
        for (const auto &p : result.photos) {
            const auto key = canonicalPath(p.path);
            if (!existing.contains(key)) { m_photos << p; existing.insert(key); ++added; }
            else ++duplicates;
        }
        endResetModel();
        m_roots.append(result.roots); m_roots.removeDuplicates(); pruneRoots();
        if (m_selected < 0 && !m_photos.isEmpty()) m_selected = 0;
        m_scanning = false;
        m_details = result.errors.join('\n');
        m_importError = result.errors.isEmpty() ? QString() : result.errors.first();
        if (result.cancelled) m_message = QStringLiteral("Carga cancelada. Se conservaron las fotografías encontradas.");
        else if (!m_importError.isEmpty()) m_message = QStringLiteral("%1 agregadas. Revisa el aviso de carga.").arg(added);
        else if (added == 0 && duplicates > 0) m_message = QStringLiteral("Las fotografías seleccionadas ya están en el lote. Selecciónalas en la lista para editarlas.");
        else if (added == 0) {
            m_importError = QStringLiteral("No se encontraron fotografías JPG o PNG. Revisa la selección y la opción Incluir subcarpetas.");
            m_message = m_importError;
        } else m_message = QStringLiteral("%1 fotografías agregadas · %2 en el lote").arg(added).arg(count());
        m_undo.clear(); m_redo.clear();
        touch();
    });
    connect(&m_export, &QFutureWatcher<ExportResult>::finished, this, [this] {
        const auto result = m_export.result();
        m_exporting = false;
        m_cancelRequested = false;
        m_exportSucceeded = result.completed;
        m_exportFiles = result.files;
        m_exportFailed = result.failed;
        m_exportProcessed = result.completed + result.failed;
        m_exportOutcome = result.cancelled ? QStringLiteral("cancelled")
            : result.failed == 0 ? QStringLiteral("success")
            : result.files > 0 ? QStringLiteral("partial") : QStringLiteral("error");
        for (auto &p : m_photos) if (p.status == QStringLiteral("Procesando")) p.status = QStringLiteral("Pendiente");
        m_message = QStringLiteral("%1%2 exportadas · %3 errores")
            .arg(result.cancelled ? QStringLiteral("Lote cancelado. ") : QStringLiteral("Lote terminado. "))
            .arg(result.completed).arg(result.failed);
        m_details.clear();
        for (const auto &p : m_photos)
            if (!p.error.isEmpty()) m_details += QStringLiteral("%1: %2\n").arg(p.relative, p.error);
        if (!result.cancelled) m_progress = 1;
        touch();
        emit exportFinished();
    });
}

Backend::~Backend() {
    m_cancel->store(true);
    m_scan.waitForFinished();
    m_export.waitForFinished();
}

const Photo *Backend::current() const {
    return m_selected >= 0 && m_selected < count() ? &m_photos.at(m_selected) : nullptr;
}
QString Backend::currentName() const { return current() ? current()->relative : QString(); }
QString Backend::currentPath() const { return current() ? QUrl::fromLocalFile(current()->path).toString() : QString(); }
QString Backend::currentWarning() const { return current() ? Imaging::resolutionWarning(current()->size, current()->crops, m_marginMm) : QString(); }
QString Backend::currentError() const { return current() ? current()->error : QString(); }
bool Backend::dirty() const {
    for (const auto &p : m_photos) if (p.status != QStringLiteral("Exportada")) return true;
    return false;
}
QVariant Backend::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count() || role != Qt::UserRole + 1) return {};
    const auto &p = m_photos.at(index.row());
    return QVariantMap{
        {"name", QFileInfo(p.path).fileName()}, {"relative", p.relative},
        {"url", QUrl::fromLocalFile(p.path)}, {"status", p.status}, {"error", p.error},
        {"warning", !Imaging::resolutionWarning(p.size, p.crops, m_marginMm).isEmpty()},
        {"dimensions", QStringLiteral("%1 × %2 px").arg(p.size.width()).arg(p.size.height())}
    };
}
QVariantList Backend::currentCrops() const {
    QVariantList list;
    const auto crops = current() ? current()->crops : Imaging::DefaultCrops;
    for (const auto &c : crops) list << QVariantMap{{"x", c.x}, {"y", c.y}, {"zoom", c.zoom}, {"rotation", c.rotation}};
    return list;
}
void Backend::touch() {
    ++m_revision;
    if (count()) emit dataChanged(index(0), index(count() - 1));
    emit changed();
}

void Backend::addUrls(const QList<QUrl> &urls, bool recursive) {
    if (busy()) return;
    QStringList paths;
    QStringList errors;
    for (const auto &url : urls) {
        if (!url.isLocalFile()) { errors << QStringLiteral("Solo se pueden agregar archivos o carpetas locales."); continue; }
        const QString path = url.toLocalFile();
        paths << path;
    }
    if (paths.isEmpty()) {
        m_importError = errors.isEmpty() ? QStringLiteral("Selecciona al menos una imagen JPG o PNG.") : errors.first();
        m_message = m_importError; m_details = errors.join('\n'); touch(); return;
    }
    m_scanning = true;
    m_details.clear(); m_importError.clear();
    m_message = QStringLiteral("Buscando fotografías…");
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto flag = m_cancel;
    const auto defaults = m_defaults;
    const auto excluded = m_destination;
    m_scan.setFuture(QtConcurrent::run([paths, recursive, flag, defaults, excluded, errors] {
        auto result = scanPhotos(paths, recursive, flag, defaults, excluded);
        result.errors.append(errors); return result;
    }));
    touch();
}
void Backend::select(int index) {
    if (m_exporting) return;
    if (index >= 0 && index < count()) { m_selected = index; touch(); }
}
Backend::History Backend::snapshot() const {
    History h; h.defaults = m_defaults; h.marginMm = m_marginMm;
    for (const auto &p : m_photos) h.crops << p.crops;
    return h;
}
void Backend::remember() {
    m_undo << snapshot();
    if (m_undo.size() > 100) m_undo.removeFirst();
    m_redo.clear();
}
void Backend::restore(const History &h) {
    if (h.crops.size() != m_photos.size()) return;
    for (int i = 0; i < count(); ++i) {
        if (m_photos[i].crops == h.crops.at(i) && m_marginMm == h.marginMm) continue;
        m_photos[i].crops = h.crops.at(i);
        m_photos[i].status = QStringLiteral("Pendiente");
        m_photos[i].error.clear();
    }
    m_defaults = h.defaults;
    m_marginMm = h.marginMm;
    if (m_preferences) m_preferences->setMarginMm(m_marginMm);
    touch();
}
void Backend::setCrop(int index, double x, double y, double zoom) {
    if (busy() || !current() || index < 0 || index > 2) return;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(zoom)) return;
    const Imaging::Crop crop{std::clamp(x, 0., 1.), std::clamp(y, 0., 1.), std::clamp(zoom, 1., 3.), current()->crops.at(index).rotation};
    if (current()->crops.at(index) == crop) return;
    remember();
    m_photos[m_selected].crops.at(index) = crop;
    m_photos[m_selected].status = QStringLiteral("Pendiente");
    m_photos[m_selected].error.clear();
    touch();
}
void Backend::setRotation(int index, int degrees) {
    if (busy() || !current() || index < 0 || index > 2 || degrees % 90 != 0) return;
    const int rotation = Imaging::normalizedRotation(degrees);
    if (current()->crops.at(index).rotation == rotation) return;
    remember();
    m_photos[m_selected].crops.at(index).rotation = rotation;
    m_photos[m_selected].status = QStringLiteral("Pendiente");
    m_photos[m_selected].error.clear();
    touch();
}

void Backend::setMarginMm(double marginMm) {
    if (busy() || !Imaging::validMargin(marginMm) || m_marginMm == marginMm) return;
    remember();
    m_marginMm = marginMm;
    if (m_preferences) m_preferences->setMarginMm(m_marginMm);
    for (auto &photo : m_photos) { photo.status = QStringLiteral("Pendiente"); photo.error.clear(); }
    m_message = QStringLiteral("Margen interior de %1 mm por lado en todas las hojas; separación entre fotos: %2 mm.")
        .arg(m_marginMm, 0, 'f', 1).arg(m_marginMm * 2, 0, 'f', 1);
    touch();
}

void Backend::resetCrops() {
    if (busy() || !current()) return;
    remember();
    m_photos[m_selected].crops = Imaging::DefaultCrops;
    m_photos[m_selected].status = QStringLiteral("Pendiente");
    m_photos[m_selected].error.clear();
    touch();
}
void Backend::setAttach8x10(bool enabled) {
    if (busy() || m_attach8x10 == enabled) return;
    m_attach8x10 = enabled;
    for (auto &photo : m_photos) { photo.status = QStringLiteral("Pendiente"); photo.error.clear(); }
    touch();
}
void Backend::applyToAll() {
    if (busy() || !current()) return;
    remember();
    m_defaults = current()->crops;
    for (auto &p : m_photos) { p.crops = m_defaults; p.status = QStringLiteral("Pendiente"); p.error.clear(); }
    m_message = QStringLiteral("Los tres encuadres se aplicaron a %1 fotografías y a las próximas que agregues.").arg(count());
    touch();
}
void Backend::removeSelected() {
    if (busy() || !current()) return;
    beginRemoveRows({}, m_selected, m_selected);
    m_photos.removeAt(m_selected);
    endRemoveRows();
    pruneRoots();
    m_selected = count() ? std::min(m_selected, count() - 1) : -1;
    m_undo.clear(); m_redo.clear();
    touch();
}
void Backend::clear() {
    if (busy()) return;
    beginResetModel();
    m_photos.clear(); m_roots.clear(); m_defaults = Imaging::DefaultCrops; m_selected = -1;
    endResetModel();
    m_undo.clear(); m_redo.clear(); m_progress = 0;
    m_message = QStringLiteral("Carga las fotografías de tu grupo para comenzar.");
    m_details.clear(); m_importError.clear(); touch();
}
void Backend::pruneRoots() {
    m_roots.erase(std::remove_if(m_roots.begin(), m_roots.end(), [this](const QString &root) {
        return std::none_of(m_photos.begin(), m_photos.end(), [&](const Photo &photo) { return containsPath(root, photo.path); });
    }), m_roots.end());
}
void Backend::undo() {
    if (!canUndo()) return;
    m_redo << snapshot(); restore(m_undo.takeLast());
}
void Backend::redo() {
    if (!canRedo()) return;
    m_undo << snapshot(); restore(m_redo.takeLast());
}
void Backend::setTargetFolder(const QUrl &url) {
    if (busy()) return;
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isDir()) {
        m_message = QStringLiteral("La carpeta de salida no existe o no está disponible. Selecciona una carpeta válida.");
        touch(); return;
    }
    const QString path = QFileInfo(url.toLocalFile()).absoluteFilePath();
    for (const auto &root : m_roots) {
        if (containsPath(root, path)) {
            m_message = QStringLiteral("Elige una carpeta de salida fuera de las carpetas de origen para no volver a importar las exportaciones.");
            touch(); return;
        }
    }
    m_destination = QDir::cleanPath(path);
    if (m_preferences) m_preferences->setDestination(m_destination);
    m_message = QStringLiteral("Destino: %1").arg(QDir::toNativeSeparators(path));
    touch();
}
void Backend::exportAll(const QString &format, bool pendingOnly) {
    if (busy() || !count()) return;
    if (m_destination.isEmpty()) { m_message = QStringLiteral("Elige una carpeta de salida antes de exportar."); touch(); return; }
    for (const auto &root : m_roots) {
        if (containsPath(root, m_destination)) { m_message = QStringLiteral("La carpeta de salida debe estar fuera del origen."); touch(); return; }
    }
    QVector<Photo> photos;
    QVector<int> indexes;
    for (int i = 0; i < count(); ++i) {
        if (pendingOnly && m_photos.at(i).status == QStringLiteral("Exportada")) continue;
        indexes << i; photos << m_photos.at(i);
    }
    if (photos.isEmpty()) { m_message = QStringLiteral("Todas las fotografías del lote ya están exportadas."); touch(); return; }
    m_exporting = true; m_progress = 0; m_details.clear();
    m_cancelRequested = false;
    m_exportTotal = photos.size(); m_exportProcessed = 0; m_exportSucceeded = 0; m_exportFailed = 0;
    m_exportFiles = 0;
    m_exportFile.clear(); m_exportOutcome.clear();
    m_message = QStringLiteral("Exportando %1 fotografías…").arg(photos.size());
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto flag = m_cancel;
    const QString directory = m_destination;
    const double marginMm = m_marginMm;
    const bool attach8x10 = m_attach8x10;
    m_export.setFuture(QtConcurrent::run([this, photos, indexes, directory, format, flag, marginMm, attach8x10] {
        return exportPhotos(photos, directory, format, flag,
            [this, indexes](int index, const QString &status, const QString &output, const QString &error) {
                QMetaObject::invokeMethod(this, [this, indexes, index, status, output, error] {
                    auto &p = m_photos[indexes.at(index)];
                    p.status = status; p.output = output; p.error = error;
                    m_exportFile = p.relative;
                    if (status != QStringLiteral("Procesando")) {
                        m_exportProcessed = index + 1;
                        if (error.isEmpty()) ++m_exportSucceeded; else ++m_exportFailed;
                    }
                    m_progress = double(m_exportProcessed) / m_exportTotal;
                    m_message = m_cancelRequested ? QStringLiteral("Cancelando… se conservarán los archivos terminados.")
                        : QStringLiteral("%1 de %2 · %3").arg(index + 1).arg(indexes.size()).arg(p.relative);
                    emit dataChanged(this->index(indexes.at(index)), this->index(indexes.at(index)));
                    emit changed();
                }, Qt::QueuedConnection);
            }, marginMm, attach8x10);
    }));
    touch();
}
void Backend::cancel() {
    if (!busy()) return;
    if (m_exporting) m_cancelRequested = true;
    m_cancel->store(true);
    m_message = QStringLiteral("Cancelando… se conservarán los archivos terminados.");
    emit changed();
}
void Backend::openDestination() {
    if (m_exporting) return;
    if (!m_destination.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(m_destination));
}
void Backend::loadInitialPhotos(const QStringList &paths) {
    if (busy()) return;
    const auto result = scanPhotos(paths, true, std::make_shared<std::atomic_bool>(false), m_defaults, m_destination);
    m_roots = result.roots;
    beginResetModel();
    m_photos = result.photos;
    endResetModel();
    m_selected = count() ? 0 : -1;
    m_message = QStringLiteral("%1 fotografías listas para revisar.").arg(count());
    m_details = result.errors.join('\n');
    m_importError = result.errors.isEmpty() ? QString() : result.errors.first();
    touch();
}

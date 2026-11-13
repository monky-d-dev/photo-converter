#include "previewprovider.h"
#include <QPainter>
#include <algorithm>

QImage PreviewProvider::requestImage(const QString &id, QSize *size, const QSize &) {
    // QML deliberately requests synchronously so the provider observes GUI state on the GUI thread.
    const auto *photo = m_backend->current();
    if (!photo) return {};
    if (m_path != photo->path) {
        QString error;
        m_source = Imaging::load(photo->path, &error, 1800);
        m_path = photo->path;
    }
    if (m_source.isNull()) return {};
    QImage result;
    if (id.startsWith("sheet/")) result = Imaging::sheet(m_source, photo->crops, 90, m_backend->marginMm());
    else if (id.section('/', 1, 1).toInt() == Imaging::PortraitCrop)
        result = Imaging::portrait8x10(m_source, photo->crops[Imaging::PortraitCrop], 90);
    else {
        const int crop = qBound(0, id.section('/', 1, 1).toInt(), 2);
        const auto &layout = Imaging::layoutSlots();
        const auto slot = std::find_if(layout.begin(), layout.end(), [crop](const auto &s) { return s.crop == crop; });
        if (slot == layout.end()) return {};
        const auto &print = slot->inches;
        const QSizeF ratio = print.size();
        result = QImage(QSize(qRound(ratio.width() * 120), qRound(ratio.height() * 120)), QImage::Format_RGB32);
        result.fill(Qt::white);
        QPainter painter(&result);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const auto rect = Imaging::imageRect(print, m_backend->marginMm()).translated(-print.topLeft());
        Imaging::paintCrop(painter, QRectF(rect.x() * 120, rect.y() * 120, rect.width() * 120, rect.height() * 120),
                           m_source, photo->crops.at(qBound(0, crop, 2)));
    }
    if (size) *size = result.size();
    return result;
}

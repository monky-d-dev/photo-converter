#include "imaging.h"
#include <QColorSpace>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QSaveFile>
#include <algorithm>
#include <cmath>

namespace Imaging {
const std::array<Slot, 7> &layoutSlots() {
    static const std::array<Slot, 7> value{{
        {{0, 0, 3, 2}, 0}, {{3, 0, 3, 2}, 0},
        {{0, 2, 3, 2}, 0}, {{3, 2, 3, 2}, 0},
        {{0, 4, 4, 3}, 1}, {{0, 7, 4, 3}, 1}, {{4, 4, 4, 6}, 2}
    }};
    return value;
}

bool validMargin(double marginMm) {
    return std::isfinite(marginMm) && marginMm >= 0 && marginMm <= MaxMarginMm
        && std::abs(marginMm * 10 - std::round(marginMm * 10)) < 1e-8;
}

QRectF imageRect(const QRectF &printInches, double marginMm) {
    const double inset = marginMm / 25.4;
    // Only internal cut lines need white space; paper edges already form the cut.
    return printInches.adjusted(printInches.left() > 0 ? inset : 0,
                                printInches.top() > 0 ? inset : 0,
                                printInches.right() < 8 ? -inset : 0,
                                printInches.bottom() < 10 ? -inset : 0);
}

QRectF cropRect(const QSize &source, const QSizeF &target, const Crop &crop) {
    if (source.isEmpty() || target.isEmpty()) return {};
    const double zoom = std::clamp(std::isfinite(crop.zoom) ? crop.zoom : 1., 1., 3.);
    const double scale = std::min(source.width() / target.width(), source.height() / target.height()) / zoom;
    const double w = target.width() * scale, h = target.height() * scale;
    const double x = std::clamp(std::isfinite(crop.x) ? crop.x : .5, 0., 1.);
    const double y = std::clamp(std::isfinite(crop.y) ? crop.y : .5, 0., 1.);
    return {x * (source.width() - w), y * (source.height() - h), w, h};
}

int normalizedRotation(int degrees) {
    return ((degrees / 90) % 4 + 4) % 4 * 90;
}

QSize rotatedSize(const QSize &source, int degrees) {
    const int rotation = normalizedRotation(degrees);
    return rotation == 90 || rotation == 270 ? QSize(source.height(), source.width()) : source;
}

void paintCrop(QPainter &painter, const QRectF &target, const QImage &source, const Crop &crop) {
    if (source.isNull() || target.isEmpty()) return;
    const int rotation = normalizedRotation(crop.rotation);
    const auto rect = cropRect(rotatedSize(source.size(), rotation), target.size(), crop);
    // Rotate before framing so landscape slots can hold a complete portrait sideways.
    // Painter transforms avoid making full-resolution rotated copies for every size.
    painter.save();
    painter.setClipRect(target, Qt::IntersectClip);
    painter.translate(target.topLeft());
    painter.scale(target.width() / rect.width(), target.height() / rect.height());
    painter.translate(-rect.left(), -rect.top());
    if (rotation == 90) painter.translate(source.height(), 0);
    else if (rotation == 180) painter.translate(source.width(), source.height());
    else if (rotation == 270) painter.translate(0, source.width());
    painter.rotate(rotation);
    painter.drawImage(QPointF(0, 0), source);
    painter.restore();
}

QImage load(const QString &path, QString *error, int maxSide) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (maxSide > 0 && reader.size().isValid()) {
        const QSize size = reader.size();
        if (std::max(size.width(), size.height()) > maxSide)
            reader.setScaledSize(size.scaled(maxSide, maxSide, Qt::KeepAspectRatio));
    }
    QImage image = reader.read();
    if (image.isNull()) {
        if (error) *error = QStringLiteral("No se pudo leer la imagen: %1").arg(reader.errorString());
        return {};
    }
    // Convert embedded profiles so previews and exports use the same print-friendly RGB space.
    if (image.colorSpace().isValid() && image.colorSpace() != QColorSpace(QColorSpace::SRgb))
        image.convertToColorSpace(QColorSpace::SRgb);
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}

QImage sheet(const QImage &source, const Crops &crops, int dpi, double marginMm) {
    if (source.isNull() || dpi < 1 || dpi > 600 || !validMargin(marginMm)) return {};
    QImage output(8 * dpi, 10 * dpi, QImage::Format_RGB32);
    if (output.isNull()) return {};
    output.fill(Qt::white);
    output.setColorSpace(QColorSpace::SRgb);
    output.setDotsPerMeterX(qRound(dpi / .0254));
    output.setDotsPerMeterY(qRound(dpi / .0254));
    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (const auto &slot : layoutSlots()) {
        // Internal separators use space inside each print without adding a paper border.
        const auto r = imageRect(slot.inches, marginMm);
        paintCrop(painter, QRectF(r.x() * dpi, r.y() * dpi, r.width() * dpi, r.height() * dpi),
                  source, crops.at(slot.crop));
    }
    return output;
}

QImage portrait8x10(const QImage &source, const Crop &crop, int dpi) {
    if (source.isNull() || dpi < 1 || dpi > 600) return {};
    QImage output(8 * dpi, 10 * dpi, QImage::Format_RGB32);
    if (output.isNull()) return {};
    output.fill(Qt::white);
    output.setColorSpace(QColorSpace::SRgb);
    output.setDotsPerMeterX(qRound(dpi / .0254));
    output.setDotsPerMeterY(qRound(dpi / .0254));
    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    paintCrop(painter, output.rect(), source, crop);
    return output;
}

QString resolutionWarning(const QSize &source, const Crops &crops, double marginMm) {
    int minimum = 100000;
    for (const auto &slot : layoutSlots()) {
        const auto &crop = crops.at(slot.crop);
        const auto printed = imageRect(slot.inches, marginMm);
        const auto rect = cropRect(rotatedSize(source, crop.rotation), printed.size(), crop);
        minimum = std::min(minimum, int(std::floor(rect.width() / printed.width())));
    }
    return minimum < Dpi ? QStringLiteral("Resolución baja: %1 ppp efectivos; se recomiendan 300 ppp.").arg(minimum) : QString();
}

bool save(const QImage &image, const QString &path, const QString &format, QString *error) {
    if (image.isNull()) { if (error) *error = QStringLiteral("No hay imagen para exportar."); return false; }
    if (format != "jpg" && format != "png" && format != "pdf") {
        if (error) *error = QStringLiteral("Formato no compatible: %1").arg(format);
        return false;
    }
    QSaveFile file(path);
    // No direct-write fallback: a failed export must never leave a partial destination.
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error = file.errorString(); return false; }
    bool ok = false;
    QString failure;
    if (format == "pdf") {
        {
            QPdfWriter writer(&file);
            writer.setResolution(Dpi);
            writer.setTitle(QStringLiteral("Fotografías de graduación · 8 × 10 pulgadas"));
            writer.setCreator(QStringLiteral("Photo Converter"));
            const QPageSize size(QSizeF(8, 10), QPageSize::Inch, QStringLiteral("8x10"), QPageSize::ExactMatch);
            writer.setPageLayout(QPageLayout(size, QPageLayout::Portrait, QMarginsF(0, 0, 0, 0), QPageLayout::Inch));
            QPainter painter(&writer);
            ok = painter.isActive();
            if (ok) {
                painter.drawImage(QRect(0, 0, writer.width(), writer.height()), image);
                ok = painter.end();
            }
        }
        failure = QStringLiteral("No se pudo generar el PDF.");
    } else {
        QImageWriter writer(&file, format.toLatin1());
        if (format == "jpg") writer.setQuality(97);
        ok = writer.write(image);
        failure = writer.errorString();
    }
    if (!ok) { file.cancelWriting(); if (error) *error = failure; return false; }
    if (!file.commit()) { if (error) *error = file.errorString(); return false; }
    return true;
}
}

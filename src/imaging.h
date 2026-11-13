#pragma once
#include <QImage>
#include <QRectF>
#include <QString>
#include <array>

class QPainter;

namespace Imaging {
constexpr int Dpi = 300;
constexpr double DefaultMarginMm = 0.5;
constexpr double MaxMarginMm = 10.0;
struct Crop {
    double x = 0.5;
    double y = 0.5;
    double zoom = 1.0;
    int rotation = 0;
    bool operator==(const Crop &other) const { return x == other.x && y == other.y && zoom == other.zoom && rotation == other.rotation; }
    bool operator!=(const Crop &other) const { return !(*this == other); }
};
constexpr int PortraitCrop = 3;
using Crops = std::array<Crop, 4>;
inline constexpr Crops DefaultCrops{{Crop{.5, .5, 1., 270}, Crop{.5, .5, 1., 270}, Crop{}, Crop{}}};
struct Slot { QRectF inches; int crop; };
const std::array<Slot, 7> &layoutSlots();
bool validMargin(double marginMm);
// Use the print's position on the 8x10 sheet to identify its outside edges.
QRectF imageRect(const QRectF &printInches, double marginMm);
QRectF cropRect(const QSize &source, const QSizeF &target, const Crop &crop);
int normalizedRotation(int degrees);
QSize rotatedSize(const QSize &source, int degrees);
void paintCrop(QPainter &painter, const QRectF &target, const QImage &source, const Crop &crop);
QImage load(const QString &path, QString *error, int maxSide = 0);
QImage sheet(const QImage &source, const Crops &crops, int dpi = Dpi, double marginMm = DefaultMarginMm);
QImage portrait8x10(const QImage &source, const Crop &crop, int dpi = Dpi);
QString resolutionWarning(const QSize &source, const Crops &crops, double marginMm = DefaultMarginMm);
bool save(const QImage &sheet, const QString &path, const QString &format, QString *error);
}

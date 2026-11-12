#pragma once
#include "imaging.h"
#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <functional>
#include <memory>

struct Photo {
    QString path;
    QString relative;
    QSize size;
    Imaging::Crops crops = Imaging::DefaultCrops;
    QString status = QStringLiteral("Pendiente");
    QString error;
    QString output;
};
using CancelFlag = std::shared_ptr<std::atomic_bool>;
struct ScanResult { QVector<Photo> photos; QStringList errors; bool cancelled = false; QStringList roots; };
struct ExportResult {
    int completed = 0;
    int failed = 0;
    int files = 0;
    bool cancelled = false;
    QJsonObject toJson() const;
};
ScanResult scanPhotos(const QStringList &paths, bool recursive, const CancelFlag &cancel,
                      const Imaging::Crops &defaults = Imaging::DefaultCrops,
                      const QString &excludedDirectory = {});
QString canonicalPath(const QString &path);
bool containsPath(const QString &parent, const QString &child);
QString exportTarget(const QString &directory, const QString &relative, const QString &format,
                     bool portrait = false);
ExportResult exportPhotos(const QVector<Photo> &photos, const QString &directory,
                          const QString &format, const CancelFlag &cancel,
                          const std::function<void(int, const QString &, const QString &, const QString &)> &progress = {},
                          double marginMm = Imaging::DefaultMarginMm, bool attach8x10 = false);

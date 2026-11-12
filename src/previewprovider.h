#pragma once
#include <QQuickImageProvider>
#include "backend.h"

class PreviewProvider : public QQuickImageProvider {
public:
    explicit PreviewProvider(Backend *backend) : QQuickImageProvider(QQuickImageProvider::Image), m_backend(backend) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
private:
    Backend *m_backend;
    QString m_path;
    QImage m_source;
};

#pragma once
#include <QObject>
#include <QSettings>

class Appearance : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(double marginMm READ marginMm WRITE setMarginMm NOTIFY marginChanged)
    Q_PROPERTY(QString destination READ destination WRITE setDestination NOTIFY destinationChanged)
public:
    explicit Appearance(const QString &settingsFile = {}, QObject *parent = nullptr);
    QString theme() const { return m_theme; }
    bool dark() const { return m_theme == QStringLiteral("dark"); }
    void setTheme(const QString &theme);
    double marginMm() const { return m_marginMm; }
    void setMarginMm(double marginMm);
    QString destination() const { return m_destination; }
    void setDestination(const QString &destination);
signals:
    void changed();
    void marginChanged();
    void destinationChanged();
private:
    QSettings m_settings;
    QString m_theme;
    double m_marginMm;
    QString m_destination;
};

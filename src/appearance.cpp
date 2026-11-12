#include "appearance.h"
#include "imaging.h"
#include <QDir>
#include <QStandardPaths>
#include <cmath>

Appearance::Appearance(const QString &settingsFile, QObject *parent)
    : QObject(parent),
      m_settings(settingsFile.isEmpty()
          ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath("appearance.ini")
          : settingsFile, QSettings::IniFormat) {
    m_theme = m_settings.value("theme", "light").toString();
    if (m_theme != "dark") m_theme = "light";
    bool ok = false;
    const double stored = m_settings.value("marginMm", Imaging::DefaultMarginMm).toDouble(&ok);
    // Keep saved fractional margins at the selector's one-decimal precision.
    m_marginMm = ok && std::isfinite(stored) && stored >= 0 && stored <= Imaging::MaxMarginMm
        ? std::round(stored * 10) / 10 : Imaging::DefaultMarginMm;
    m_destination = m_settings.value("destination").toString();
}

void Appearance::setTheme(const QString &theme) {
    if ((theme != "light" && theme != "dark") || m_theme == theme) return;
    m_theme = theme;
    m_settings.setValue("theme", theme);
    m_settings.sync();
    emit changed();
}

void Appearance::setMarginMm(double marginMm) {
    if (!Imaging::validMargin(marginMm) || m_marginMm == marginMm) return;
    m_marginMm = marginMm;
    m_settings.setValue("marginMm", marginMm);
    m_settings.sync();
    emit marginChanged();
}

void Appearance::setDestination(const QString &destination) {
    if (m_destination == destination) return;
    m_destination = destination;
    m_settings.setValue("destination", destination);
    m_settings.sync();
    emit destinationChanged();
}

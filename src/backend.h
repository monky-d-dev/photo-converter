#pragma once
#include "batch.h"
#include <QFutureWatcher>
#include <QAbstractListModel>
#include <QUrl>
#include <QVariantList>

class Appearance;

class Backend : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* photos READ photos CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int selected READ selected NOTIFY changed)
    Q_PROPERTY(QString currentName READ currentName NOTIFY changed)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY changed)
    Q_PROPERTY(QString currentWarning READ currentWarning NOTIFY changed)
    Q_PROPERTY(QString currentError READ currentError NOTIFY changed)
    Q_PROPERTY(QVariantList currentCrops READ currentCrops NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool exporting READ exporting NOTIFY changed)
    Q_PROPERTY(bool cancelRequested READ cancelRequested NOTIFY changed)
    Q_PROPERTY(int exportTotal READ exportTotal NOTIFY changed)
    Q_PROPERTY(int exportProcessed READ exportProcessed NOTIFY changed)
    Q_PROPERTY(int exportSucceeded READ exportSucceeded NOTIFY changed)
    Q_PROPERTY(int exportFiles READ exportFiles NOTIFY changed)
    Q_PROPERTY(int exportFailed READ exportFailed NOTIFY changed)
    Q_PROPERTY(QString exportFile READ exportFile NOTIFY changed)
    Q_PROPERTY(QString exportOutcome READ exportOutcome NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString details READ details NOTIFY changed)
    Q_PROPERTY(QString destination READ destination NOTIFY changed)
    Q_PROPERTY(QUrl destinationUrl READ destinationUrl NOTIFY changed)
    Q_PROPERTY(QString importError READ importError NOTIFY changed)
    Q_PROPERTY(double marginMm READ marginMm WRITE setMarginMm NOTIFY changed)
    Q_PROPERTY(bool attach8x10 READ attach8x10 WRITE setAttach8x10 NOTIFY changed)

public:
    explicit Backend(QObject *parent = nullptr, Appearance *preferences = nullptr);
    ~Backend() override;
    QAbstractItemModel *photos() { return this; }
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : count(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override { return {{Qt::UserRole + 1, "photoData"}}; }
    int count() const { return m_photos.size(); }
    int selected() const { return m_selected; }
    QString currentName() const;
    QString currentPath() const;
    QString currentWarning() const;
    QString currentError() const;
    QVariantList currentCrops() const;
    int revision() const { return m_revision; }
    bool busy() const { return m_scanning || m_exporting; }
    bool exporting() const { return m_exporting; }
    bool cancelRequested() const { return m_cancelRequested; }
    int exportTotal() const { return m_exportTotal; }
    int exportProcessed() const { return m_exportProcessed; }
    int exportSucceeded() const { return m_exportSucceeded; }
    int exportFiles() const { return m_exportFiles; }
    int exportFailed() const { return m_exportFailed; }
    QString exportFile() const { return m_exportFile; }
    QString exportOutcome() const { return m_exportOutcome; }
    bool scanning() const { return m_scanning; }
    bool dirty() const;
    bool canUndo() const { return !busy() && !m_undo.isEmpty(); }
    bool canRedo() const { return !busy() && !m_redo.isEmpty(); }
    double progress() const { return m_progress; }
    QString message() const { return m_message; }
    QString details() const { return m_details; }
    QString destination() const { return m_destination; }
    QUrl destinationUrl() const { return m_destination.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_destination); }
    QString importError() const { return m_importError; }
    double marginMm() const { return m_marginMm; }
    bool attach8x10() const { return m_attach8x10; }
    const Photo *current() const;

    Q_INVOKABLE void addUrls(const QList<QUrl> &urls, bool recursive);
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void setCrop(int index, double x, double y, double zoom);
    Q_INVOKABLE void setRotation(int index, int degrees);
    Q_INVOKABLE void setMarginMm(double marginMm);
    Q_INVOKABLE void setAttach8x10(bool enabled);
    Q_INVOKABLE void resetCrops();
    Q_INVOKABLE void applyToAll();
    Q_INVOKABLE void removeSelected();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void setTargetFolder(const QUrl &url);
    Q_INVOKABLE void exportAll(const QString &format, bool pendingOnly);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void openDestination();
    void loadInitialPhotos(const QStringList &paths);

signals:
    void changed();
    void exportFinished();

private:
    struct History { QVector<Imaging::Crops> crops; Imaging::Crops defaults; double marginMm; };
    History snapshot() const;
    void remember();
    void restore(const History &history);
    void touch();
    void pruneRoots();
    QVector<Photo> m_photos;
    QStringList m_roots;
    Imaging::Crops m_defaults = Imaging::DefaultCrops;
    double m_marginMm = Imaging::DefaultMarginMm;
    bool m_attach8x10 = false;
    Appearance *m_preferences = nullptr;
    int m_selected = -1;
    int m_revision = 0;
    bool m_scanning = false;
    bool m_exporting = false;
    bool m_cancelRequested = false;
    int m_exportTotal = 0;
    int m_exportProcessed = 0;
    int m_exportSucceeded = 0;
    int m_exportFiles = 0;
    int m_exportFailed = 0;
    QString m_exportFile;
    QString m_exportOutcome;
    double m_progress = 0;
    QString m_message = QStringLiteral("Carga las fotografías de tu grupo para comenzar.");
    QString m_details;
    QString m_destination;
    QString m_importError;
    CancelFlag m_cancel = std::make_shared<std::atomic_bool>(false);
    QFutureWatcher<ScanResult> m_scan;
    QFutureWatcher<ExportResult> m_export;
    QVector<History> m_undo, m_redo;
};

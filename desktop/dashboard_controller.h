#ifndef DASHBOARD_CONTROLLER_H
#define DASHBOARD_CONTROLLER_H

#include "application_service.h"
#include "archive_service.h"
#include "vault_service.h"

#include <QObject>
#include <QFutureWatcher>
#include <QUrl>
#include <QString>
#include <QVariantList>

class DashboardController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(qulonglong archiveCount READ archiveCount NOTIFY summaryChanged)
    Q_PROPERTY(QString spaceSaved READ spaceSaved NOTIFY summaryChanged)
    Q_PROPERTY(QString originalData READ originalData NOTIFY summaryChanged)
    Q_PROPERTY(QString freeSpace READ freeSpace NOTIFY summaryChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList archives READ archives NOTIFY summaryChanged)
    Q_PROPERTY(QString archivesStatus READ archivesStatus NOTIFY summaryChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanChanged)
    Q_PROPERTY(QString scanSummary READ scanSummary NOTIFY scanChanged)
    Q_PROPERTY(bool restoring READ restoring NOTIFY restoreChanged)
    Q_PROPERTY(QString restoreStatus READ restoreStatus NOTIFY restoreChanged)
    Q_PROPERTY(QString restoredPath READ restoredPath NOTIFY restoreChanged)
    Q_PROPERTY(QVariantList scanFiles READ scanFiles NOTIFY summaryChanged)
    Q_PROPERTY(int totalScanFiles READ totalScanFiles NOTIFY summaryChanged)
    Q_PROPERTY(int activeScanFiles READ activeScanFiles NOTIFY summaryChanged)
    Q_PROPERTY(int inactiveScanFiles READ inactiveScanFiles NOTIFY summaryChanged)
    Q_PROPERTY(int skippedScanFiles READ skippedScanFiles NOTIFY summaryChanged)
    Q_PROPERTY(QString filesStatus READ filesStatus NOTIFY summaryChanged)
    Q_PROPERTY(int defaultInactivityDays READ defaultInactivityDays NOTIFY settingsChanged)
    Q_PROPERTY(QString vaultLocation READ vaultLocation NOTIFY settingsChanged)
    Q_PROPERTY(QString settingsStatus READ settingsStatus NOTIFY settingsChanged)
    Q_PROPERTY(bool deleting READ deleting NOTIFY deleteChanged)
    Q_PROPERTY(QString deleteStatus READ deleteStatus NOTIFY deleteChanged)

public:
    explicit DashboardController(QObject *parent = nullptr);

    qulonglong archiveCount() const;
    QString spaceSaved() const;
    QString originalData() const;
    QString freeSpace() const;
    QString statusMessage() const;
    QVariantList archives() const;
    QString archivesStatus() const;

    bool scanning() const;
    QString scanStatus() const;
    QString scanSummary() const;

    bool restoring() const;
    QString restoreStatus() const;
    QString restoredPath() const;

    QVariantList scanFiles() const;
    int totalScanFiles() const;
    int activeScanFiles() const;
    int inactiveScanFiles() const;
    int skippedScanFiles() const;
    QString filesStatus() const;
    int defaultInactivityDays() const;
    QString vaultLocation() const;
    QString settingsStatus() const;
    bool deleting() const;
    QString deleteStatus() const;

    Q_INVOKABLE void setDefaultInactivityDays(int days);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startScan(const QUrl& folderUrl, int inactiveDays);
    Q_INVOKABLE void restoreSelectedArchive(const QString& archiveId, const QUrl& destinationUrl);
    Q_INVOKABLE void deleteSelectedArchive(const QString& archiveId);

signals:
    void summaryChanged();
    void scanChanged();
    void restoreChanged();
    void settingsChanged();
    void deleteChanged();

private:
    qulonglong archiveCount_ = 0;
    QString spaceSaved_ = "—";
    QString originalData_ = "—";
    QString freeSpace_ = "—";
    QString statusMessage_ = "Loading vault information...";
    QVariantList archives_;
    QString archivesStatus_ = "Archives have not been loaded yet.";

    bool scanning_ = false;
    QString scanStatus_ = "Choose a folder to begin.";
    QString scanSummary_;
    QFutureWatcher<ApplicationResult> scanWatcher_;

    bool restoring_ = false;
    QString restoreStatus_ = "Select an archive to restore.";
    QString restoredPath_;
    QFutureWatcher<ArchiveRestoreResult> restoreWatcher_;

    bool deleting_ = false;
    QString deleteStatus_ = "Select an archive to permanently delete.";
    QFutureWatcher<ArchiveDeletionResult> deleteWatcher_;

    QVariantList scanFiles_;
    int totalScanFiles_ = 0;
    int activeScanFiles_ = 0;
    int inactiveScanFiles_ = 0;
    int skippedScanFiles_ = 0;
    QString filesStatus_ = "Run a scan to view file information.";

    void loadScanReport(const QString& projectRoot);

    int defaultInactivityDays_ = 30;
    QString settingsStatus_ = "Preferences loaded.";
};

#endif

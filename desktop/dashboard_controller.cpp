#include "dashboard_controller.h"

#include "vault_service.h"
#include "archive.h"
#include "vault_report.h"
#include "scan_report.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QVariantMap>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstdint>
#include <QtConcurrent/QtConcurrentRun>

namespace
{
QString findProjectRoot()
{
    QStringList startingPaths = {
        QCoreApplication::applicationDirPath(),
        QDir::currentPath()
    };

    for (const QString& path : startingPaths) {
        QDir dir(path);

        for (int i = 0; i < 10; ++i) {
            if (QFileInfo::exists(dir.filePath("CMakeLists.txt")) &&
                QFileInfo::exists(dir.filePath("src/vault_service.cpp"))) {
                return dir.absolutePath();
            }

            if (!dir.cdUp()) {
                break;
            }
        }
    }

    return QString();
}

QString formatBytes(std::uintmax_t bytes)
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};

    double size = static_cast<double>(bytes);
    int unit = 0;

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        ++unit;
    }

    return QString::number(size, 'f', unit == 0 ? 0 : 2)
           + " " + units[unit];
}
}

DashboardController::DashboardController(QObject *parent)
    : QObject(parent)
{
    QSettings settings("SmartFileVault", "SmartFileVault");
    defaultInactivityDays_ = settings.value("defaultInactivityDays", 30).toInt();

    if (defaultInactivityDays_ < 1 || defaultInactivityDays_ > 365) {
        defaultInactivityDays_ = 30;
    }

    connect(&scanWatcher_,
            &QFutureWatcher<ApplicationResult>::finished,
            this,
            [this]() {
                const ApplicationResult result = scanWatcher_.result();

                scanning_ = false;

                if (result.success) {
                    scanStatus_ = "Scan and archive operation completed.";

                    scanSummary_ =
                        QString("Files scanned: %1\n"
                                "Files skipped during scan: %2\n"
                                "Inactive files: %3\n"
                                "Successfully archived: %4\n"
                                "Skipped during processing: %5\n"
                                "Space saved: %6")
                            .arg(result.scannedFiles)
                            .arg(result.scanSkippedFiles)
                            .arg(result.processResult.inactiveFiles)
                            .arg(result.processResult.archivedFiles)
                            .arg(result.processResult.skippedFiles)
                            .arg(formatBytes(result.processResult.spaceSaved));

                    refresh();
                } else {
                    scanStatus_ =
                        "The operation could not be completed. Check folder access and try again.";
                    scanSummary_.clear();
                }

                emit scanChanged();
            });

    connect(&restoreWatcher_,
            &QFutureWatcher<ArchiveRestoreResult>::finished,
            this,
            [this]() {
                const ArchiveRestoreResult result = restoreWatcher_.result();

                restoring_ = false;

                if (result.restored) {
                    restoredPath_ = QString::fromStdString(result.restoredPath);

                    if (result.archiveDeleted &&
                        result.reportUpdated &&
                        !result.partialFailure) {
                        restoreStatus_ =
                            "Restore completed successfully. Archive removed from the vault.";
                    } else {
                        restoreStatus_ =
                            "File restored, but archive cleanup or report update was incomplete.";

                        if (!result.message.empty()) {
                            restoreStatus_ += " " +
                                QString::fromStdString(result.message);
                        }

                        if (!result.reportUpdated) {
                            restoreStatus_ +=
                                " The vault report could not be updated.";
                        }
                    }

                    // Reload the archive list even when cleanup was partial.
                    refresh();
                } else {
                    restoredPath_.clear();
                    restoreStatus_ = result.message.empty()
                        ? "Restore failed. The archive was preserved."
                        : QString::fromStdString(result.message);
                }

                emit restoreChanged();
            });

    connect(&deleteWatcher_,
            &QFutureWatcher<ArchiveDeletionResult>::finished,
            this,
            [this]() {
                const ArchiveDeletionResult result = deleteWatcher_.result();
                deleting_ = false;

                if (result.archiveDeleted) {
                    refresh();

                    if (result.originalDeleted) {
                        deleteStatus_ = "Archive and verified original file were permanently deleted.";
                    } else if (result.originalMissing) {
                        deleteStatus_ = "Archive deleted. The original file was already missing.";
                    } else if (result.originalPreserved) {
                        deleteStatus_ = "Archive deleted. The original file was preserved because it could not be safely verified.";
                    } else {
                        deleteStatus_ = "Archive deleted. Original file status: "
                                        + QString::fromStdString(result.originalStatus);
                    }

                    if (result.partialFailure && !result.message.empty()) {
                        deleteStatus_ += "\n" + QString::fromStdString(result.message);
                    }
                } else {
                    deleteStatus_ = result.message.empty()
                        ? "Deletion failed. The archive could not be removed."
                        : QString::fromStdString(result.message);
                }

                emit deleteChanged();
            });
}

qulonglong DashboardController::archiveCount() const
{
    return archiveCount_;
}

QString DashboardController::spaceSaved() const
{
    return spaceSaved_;
}

QString DashboardController::originalData() const
{
    return originalData_;
}

QString DashboardController::freeSpace() const
{
    return freeSpace_;
}

QString DashboardController::statusMessage() const
{
    return statusMessage_;
}

QVariantList DashboardController::archives() const
{
    return archives_;
}

QString DashboardController::archivesStatus() const
{
    return archivesStatus_;
}

bool DashboardController::scanning() const
{
    return scanning_;
}

QString DashboardController::scanStatus() const
{
    return scanStatus_;
}

QString DashboardController::scanSummary() const
{
    return scanSummary_;
}

bool DashboardController::restoring() const
{
    return restoring_;
}

QString DashboardController::restoreStatus() const
{
    return restoreStatus_;
}

QString DashboardController::restoredPath() const
{
    return restoredPath_;
}

QVariantList DashboardController::scanFiles() const
{
    return scanFiles_;
}

int DashboardController::totalScanFiles() const
{
    return totalScanFiles_;
}

int DashboardController::activeScanFiles() const
{
    return activeScanFiles_;
}

int DashboardController::inactiveScanFiles() const
{
    return inactiveScanFiles_;
}

int DashboardController::skippedScanFiles() const
{
    return skippedScanFiles_;
}

QString DashboardController::filesStatus() const
{
    return filesStatus_;
}

int DashboardController::defaultInactivityDays() const
{
    return defaultInactivityDays_;
}

QString DashboardController::vaultLocation() const
{
    const QString root = findProjectRoot();

    if (root.isEmpty()) {
        return "Unavailable";
    }

    return QDir(root).filePath("SmartFileVault_Vault");
}

QString DashboardController::settingsStatus() const
{
    return settingsStatus_;
}

bool DashboardController::deleting() const
{
    return deleting_;
}

QString DashboardController::deleteStatus() const
{
    return deleteStatus_;
}

void DashboardController::setDefaultInactivityDays(int days)
{
    if (days < 1 || days > 365) {
        settingsStatus_ = "Choose a value between 1 and 365 days.";
        emit settingsChanged();
        return;
    }

    defaultInactivityDays_ = days;

    QSettings settings("SmartFileVault", "SmartFileVault");
    settings.setValue("defaultInactivityDays", days);
    settings.sync();

    settingsStatus_ = "Your default inactivity period has been saved.";
    emit settingsChanged();
}

void DashboardController::startScan(const QUrl& folderUrl, int inactiveDays)
{
    if (scanning_ || restoring_ || deleting_) {
        return;
    }

    if (!folderUrl.isLocalFile() || inactiveDays <= 0) {
        scanStatus_ = "Select a valid local folder and inactivity period.";
        scanSummary_.clear();
        emit scanChanged();
        return;
    }

    const QString folderPath = QFileInfo(folderUrl.toLocalFile()).canonicalFilePath();

    if (folderPath.isEmpty() || !QFileInfo(folderPath).isDir()) {
        scanStatus_ = "The selected folder does not exist or cannot be accessed.";
        scanSummary_.clear();
        emit scanChanged();
        return;
    }

    const QString projectRoot = findProjectRoot();

    if (projectRoot.isEmpty()) {
        scanStatus_ = "Could not locate the SmartFileVault project folder.";
        emit scanChanged();
        return;
    }

    const QString vaultPath =
        QFileInfo(QDir(projectRoot).filePath("SmartFileVault_Vault"))
            .canonicalFilePath();

    if (vaultPath.isEmpty()) {
        scanStatus_ = "The existing local vault could not be located.";
        emit scanChanged();
        return;
    }

    const QString folderPrefix =
        folderPath.endsWith(QDir::separator())
            ? folderPath
            : folderPath + QDir::separator();

    const QString vaultPrefix = vaultPath + QDir::separator();

    // Prevent scanning the vault or a folder containing the vault.
    if (folderPath == vaultPath ||
        folderPath.startsWith(vaultPrefix) ||
        vaultPath.startsWith(folderPrefix)) {
        scanStatus_ =
            "This folder overlaps with the SmartFileVault vault. Choose another folder.";
        scanSummary_.clear();
        emit scanChanged();
        return;
    }

    const QString archiveRoot =
        QDir(projectRoot).filePath("SmartFileVault_Vault");

    scanning_ = true;
    scanStatus_ = "Scanning folder and archiving eligible inactive files...";
    scanSummary_.clear();
    emit scanChanged();

    scanWatcher_.setFuture(
        QtConcurrent::run([folderPath, inactiveDays, archiveRoot]() {
            ApplicationResult result{};

            runScanAndArchive(
                folderPath.toStdString(),
                inactiveDays,
                archiveRoot.toStdString(),
                result
            );

            return result;
        })
    );
}

void DashboardController::restoreSelectedArchive(
    const QString& archiveId,
    const QUrl& destinationUrl
)
{
    if (scanning_ || restoring_ || deleting_) {
        restoreStatus_ = "Wait for the current operation to finish.";
        emit restoreChanged();
        return;
    }

    if (archiveId.trimmed().isEmpty() ||
        !destinationUrl.isLocalFile()) {
        restoreStatus_ = "Select a valid archive and destination folder.";
        emit restoreChanged();
        return;
    }

    const QString destinationPath =
        QFileInfo(destinationUrl.toLocalFile()).canonicalFilePath();

    if (destinationPath.isEmpty() ||
        !QFileInfo(destinationPath).isDir()) {
        restoreStatus_ =
            "The selected destination folder does not exist or cannot be accessed.";
        emit restoreChanged();
        return;
    }

    const QString projectRoot = findProjectRoot();

    if (projectRoot.isEmpty()) {
        restoreStatus_ = "Could not locate the SmartFileVault project folder.";
        emit restoreChanged();
        return;
    }

    const QString archiveRoot =
        QDir(projectRoot).filePath("SmartFileVault_Vault");

    if (!QFileInfo(archiveRoot).isDir()) {
        restoreStatus_ = "The local vault folder could not be found.";
        emit restoreChanged();
        return;
    }

    restoring_ = true;
    restoredPath_.clear();
    restoreStatus_ = "Restoring selected archive...";
    emit restoreChanged();

    restoreWatcher_.setFuture(
        QtConcurrent::run([archiveRoot, archiveId, destinationPath]() {
            ArchiveRestoreResult result{};

            const bool restored = restoreFromArchive(
                archiveRoot.toStdString(),
                archiveId.toStdString(),
                destinationPath.toStdString(),
                result
            );

            if (!restored || !result.restored) {
                return result;
            }

            const std::string reportPath =
                archiveRoot.toStdString() + "/vault_report.json";

            result.reportUpdated = saveVaultReport(
                archiveRoot.toStdString(),
                reportPath
            );

            if (!result.reportUpdated) {
                result.partialFailure = true;

                if (!result.message.empty()) {
                    result.message += " ";
                }

                result.message +=
                    "The restored file is safe, but vault_report.json could not be updated.";
            }

            return result;
        })
    );
}


void DashboardController::deleteSelectedArchive(const QString& archiveId)
{
    if (scanning_ || restoring_ || deleting_) {
        deleteStatus_ = "Wait for the current operation to finish.";
        emit deleteChanged();
        return;
    }

    if (archiveId.trimmed().isEmpty()) {
        deleteStatus_ = "Select a valid archive to delete.";
        emit deleteChanged();
        return;
    }

    const QString projectRoot = findProjectRoot();

    if (projectRoot.isEmpty()) {
        deleteStatus_ = "Could not locate the SmartFileVault project folder.";
        emit deleteChanged();
        return;
    }

    const QString archiveRoot =
        QDir(projectRoot).filePath("SmartFileVault_Vault");

    if (!QFileInfo(archiveRoot).isDir()) {
        deleteStatus_ = "The local vault folder could not be found.";
        emit deleteChanged();
        return;
    }

    deleting_ = true;
    deleteStatus_ = "Permanently deleting the selected archive...";
    emit deleteChanged();

    deleteWatcher_.setFuture(
        QtConcurrent::run([archiveRoot, archiveId]() {
            ArchiveDeletionResult result{};

            const bool deletionSucceeded = deleteArchiveFromVault(
                archiveRoot.toStdString(),
                archiveId.toStdString(),
                result
            );

            std::string scanReportError;
            result.scanReportUpdated = reconcileScanReportAfterPermanentDelete(
                QDir(archiveRoot).filePath("scan_report.json").toStdString(),
                result,
                scanReportError
            );
            if (!result.scanReportUpdated) {
                result.partialFailure = true;
                result.errorCode = "scan_report_update_failed";
                if (!result.message.empty()) result.message += " ";
                result.message += scanReportError;
            }

            if (result.archiveDeleted) {
                const QString reportPath =
                    QDir(archiveRoot).filePath("vault_report.json");

                const bool reportSaved = saveVaultReport(
                    archiveRoot.toStdString(),
                    reportPath.toStdString()
                );

                if (!reportSaved) {
                    result.partialFailure = true;

                    if (!result.message.empty()) {
                        result.message += " ";
                    }

                    result.message +=
                        "The archive was deleted, but vault_report.json could not be updated.";
                    if (result.errorCode.empty()) result.errorCode = "vault_report_update_failed";
                }
            }

            return result;
        })
    );
}

void DashboardController::loadScanReport(const QString& projectRoot)
{
    const QString reportPath =
        QDir(projectRoot).filePath(
            "SmartFileVault_Vault/scan_report.json"
        );

    QFile reportFile(reportPath);

    if (!reportFile.exists()) {
        scanFiles_.clear();
        totalScanFiles_ = 0;
        activeScanFiles_ = 0;
        inactiveScanFiles_ = 0;
        skippedScanFiles_ = 0;
        filesStatus_ = "No scan report available. Run a scan first.";
        return;
    }

    if (!reportFile.open(QIODevice::ReadOnly)) {
        filesStatus_ = "Could not open the latest scan report.";
        return;
    }

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(reportFile.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        filesStatus_ = "The scan report could not be read.";
        return;
    }

    const QJsonObject root = document.object();

    totalScanFiles_ = root.value("totalFiles").toInt();
    activeScanFiles_ = root.value("activeFiles").toInt();
    inactiveScanFiles_ = root.value("inactiveFiles").toInt();
    skippedScanFiles_ = root.value("skippedFiles").toInt();

    QVariantList items;

    const QJsonArray fileArray = root.value("files").toArray();

    for (const QJsonValue& value : fileArray) {
        const QJsonObject file = value.toObject();

        QVariantMap item;

        item["fileName"] = file.value("fileName").toString();
        item["filePath"] = file.value("filePath").toString();
        item["extension"] = file.value("extension").toString();
        item["sizeText"] = file.value("sizeText").toString();
        item["status"] = file.value("status").toString();

        const qint64 modified =
            file.value("lastModified").toVariant().toLongLong();

        const QDateTime modifiedDate =
            QDateTime::fromSecsSinceEpoch(modified);

        item["lastModifiedText"] =
            modifiedDate.isValid()
                ? modifiedDate.toLocalTime().toString("dd MMM yyyy, hh:mm AP")
                : "Unknown";

        items.append(item);
    }

    scanFiles_ = items;

    filesStatus_ =
        QString("%1 file records loaded from the latest scan.")
            .arg(scanFiles_.size());
}

void DashboardController::refresh()
{
    const QString projectRoot = findProjectRoot();

    if (projectRoot.isEmpty()) {
        statusMessage_ = "Could not locate the SmartFileVault project folder.";
        emit summaryChanged();
        return;
    }

    const QString vaultPath =
        QDir(projectRoot).filePath("SmartFileVault_Vault");

    if (!QFileInfo(vaultPath).isDir()) {
        statusMessage_ = "The existing SmartFileVault vault folder was not found.";
        emit summaryChanged();
        return;
    }

    VaultSummary summary{};

    if (!getVaultSummary(vaultPath.toStdString(), summary)) {
        statusMessage_ = "Could not read vault storage information.";
        emit summaryChanged();
        return;
    }

    archiveCount_ = static_cast<qulonglong>(summary.archiveCount);
    spaceSaved_ = formatBytes(summary.spaceSaved);
    originalData_ = formatBytes(summary.originalSize);
    freeSpace_ = formatBytes(summary.freeSpace);

    statusMessage_ = "Live information loaded from your existing local vault.";

    const std::vector<ArchiveMetadata> metadataList =
        getAllArchives(vaultPath.toStdString());

    QVariantList archiveItems;

    for (const ArchiveMetadata& metadata : metadataList) {
        QVariantMap item;
        item["archiveId"] = QString::fromStdString(metadata.archiveId);
        item["originalName"] = QString::fromStdString(metadata.originalName);
        item["originalSize"] = formatBytes(metadata.originalSize);
        item["compressedSize"] = formatBytes(metadata.compressedSize);
        item["compressionPercentage"] =
            QString::number(metadata.compressionPercentage, 'f', 2) + "%";
        item["archivedAt"] =
            QDateTime::fromSecsSinceEpoch(metadata.archivedAt)
                .toLocalTime().toString("dd MMM yyyy, hh:mm AP");

        archiveItems.append(item);
    }

    archives_ = archiveItems;
    archivesStatus_ = archives_.isEmpty()
        ? "No archived files found."
        : QString("%1 archived files loaded from your local vault.")
              .arg(archives_.size());

    loadScanReport(projectRoot);

    emit summaryChanged();
}

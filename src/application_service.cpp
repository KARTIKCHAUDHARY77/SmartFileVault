
#include "application_service.h"
#include "scan_report.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

#ifdef SFV_ENABLE_TEST_HOOK
namespace { void (*scanBeforeReportPublishTestHook)() = nullptr; }
void setScanBeforeReportPublishTestHook(void (*hook)())
{
    scanBeforeReportPublishTestHook = hook;
}
#endif

bool runScanAndArchive(const std::string& directoryPath,
                       int inactiveDays,
                       const std::string& archiveRoot,
                       ApplicationResult& result)
{
    result.success = false;
    result.scannedFiles = 0;
    result.scanSkippedFiles = 0;
    result.processResult = {};

    FileScanResult scanResult;

    // Make sure the report directory exists before locking the report.
    std::error_code directoryError;

    fs::create_directories(archiveRoot, directoryError);

    if (directoryError) {
        return false;
    }

    fs::path reportPath =
        fs::path(archiveRoot) / "scan_report.json";

    {
        // Hold the report lock from before the scan until the snapshot is
        // published, so a permanent-delete reconciliation cannot be
        // overwritten by an older scan snapshot. The lock is released at the
        // end of this scope, before processInactiveFiles() runs.
        ScanReportLock reportLock(reportPath.string());

        if (!reportLock.locked()) {
            return false;
        }

        // Scan the selected directory first.
        if (!scanDirectory(directoryPath, inactiveDays, scanResult, archiveRoot)) {
            return false;
        }

        result.scannedFiles =
            static_cast<int>(scanResult.files.size());

        result.scanSkippedFiles =
            scanResult.skippedFiles;

#ifdef SFV_ENABLE_TEST_HOOK
        if (scanBeforeReportPublishTestHook) {
            scanBeforeReportPublishTestHook();
        }
#endif

        // Save the scan snapshot for the API and frontend. The lock is
        // already held, so use the non-locking writer.
        if (!saveScanReportLocked(scanResult, reportPath.string())) {
            return false;
        }
    }

    // Process inactive files after saving the scan snapshot and releasing
    // the report lock (the vault has its own mutex; never hold both).
    if (!processInactiveFiles(
            scanResult.files,
            archiveRoot,
            result.processResult)) {
        return false;
    }

    result.success = true;

    return true;
}


#include "application_service.h"
#include "scan_report.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

bool runScanAndArchive(const std::string& directoryPath,
                       int inactiveDays,
                       const std::string& archiveRoot,
                       ApplicationResult& result)
{
    result.success = false;
    result.scannedFiles = 0;
    result.scanSkippedFiles = 0;
    result.processResult = {};

    std::error_code directoryError;
    fs::create_directories(archiveRoot, directoryError);
    if (directoryError) {
        return false;
    }

    const fs::path reportPath =
        fs::path(archiveRoot) / "scan_report.json";

    FileScanResult scanResult;

    {
        ScanReportLock reportLock(reportPath.string());
        if (!reportLock.valid()) {
            return false;
        }

        if (!scanDirectory(directoryPath, inactiveDays,
                           scanResult, archiveRoot)) {
            return false;
        }

        result.scannedFiles =
            static_cast<int>(scanResult.files.size());
        result.scanSkippedFiles = scanResult.skippedFiles;

        if (!saveScanReportLocked(scanResult, reportPath.string())) {
            return false;
        }
    }

    // Release the report lock before processing; vault operations lock internally.
    if (!processInactiveFiles(
            scanResult.files,
            archiveRoot,
            result.processResult)) {
        return false;
    }

    result.success = true;
    return true;
}

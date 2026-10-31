
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

    FileScanResult scanResult;

    // Scan the selected directory first.
    if (!scanDirectory(directoryPath, inactiveDays, scanResult)) {
        return false;
    }

    result.scannedFiles =
        static_cast<int>(scanResult.files.size());

    result.scanSkippedFiles =
        scanResult.skippedFiles;

    // Make sure the report directory exists.
    std::error_code directoryError;

    fs::create_directories(archiveRoot, directoryError);

    if (directoryError) {
        return false;
    }

    // Save the scan snapshot for the API and frontend.
    fs::path reportPath =
        fs::path(archiveRoot) / "scan_report.json";

    if (!saveScanReport(scanResult, reportPath.string())) {
        return false;
    }

    // Process inactive files after saving the scan snapshot.
    if (!processInactiveFiles(
            scanResult.files,
            archiveRoot,
            result.processResult)) {
        return false;
    }

    result.success = true;

    return true;
}

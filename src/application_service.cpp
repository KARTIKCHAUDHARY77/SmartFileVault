#include "application_service.h"

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

    // First scan the directory
    if (!scanDirectory(directoryPath, inactiveDays, scanResult)) {
        return false;
    }

    result.scannedFiles =
        static_cast<int>(scanResult.files.size());

    result.scanSkippedFiles =
        scanResult.skippedFiles;

    // Process the inactive files
    if (!processInactiveFiles(
            scanResult.files,
            archiveRoot,
            result.processResult)) {
        return false;
    }

    result.success = true;

    return true;
}
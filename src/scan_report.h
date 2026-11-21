#ifndef SCAN_REPORT_H
#define SCAN_REPORT_H

#include "file_manager.h"
#include "archive_service.h"

#include <string>

bool saveScanReport(const FileScanResult& result,
                    const std::string& outputPath);

// Remove the exact original-path record only after both archive artifacts are
// gone and the original is deleted or already missing. Preserved files remain.
bool reconcileScanReportAfterPermanentDelete(
    const std::string& reportPath,
    const ArchiveDeletionResult& deletion,
    std::string& errorMessage
);

#endif
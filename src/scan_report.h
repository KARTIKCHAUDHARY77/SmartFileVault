#ifndef SCAN_REPORT_H
#define SCAN_REPORT_H

#include "file_manager.h"
#include "archive_service.h"

#include <string>

class ScanReportLock {
public:
    explicit ScanReportLock(const std::string& reportPath);
    ~ScanReportLock();

    ScanReportLock(const ScanReportLock&) = delete;
    ScanReportLock& operator=(const ScanReportLock&) = delete;

    bool valid() const;

private:
    int lockFd = -1;
};

bool saveScanReport(const FileScanResult& result,
                    const std::string& outputPath);

// Caller must hold ScanReportLock for the same report path.
bool saveScanReportLocked(const FileScanResult& result,
                          const std::string& outputPath);

// Remove the exact original-path record only after both archive artifacts are
// gone and the original is deleted or already missing. Preserved files remain.
bool reconcileScanReportAfterPermanentDelete(
    const std::string& reportPath,
    const ArchiveDeletionResult& deletion,
    std::string& errorMessage
);


#ifdef SFV_ENABLE_TEST_HOOK
// Test-only hook for simulating scan-report persistence failures.
void setScanReportWriteTestHook(bool (*hook)());
void setScanReportLockBlockedTestHook(void (*hook)());
#endif

#endif
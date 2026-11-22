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

    bool valid() const { return fd_ >= 0; }
    bool locked() const { return fd_ >= 0; }

    const std::string& errorMessage() const { return error_; }

    static std::string lockPathFor(const std::string& reportPath);

private:
    int fd_ = -1;
    std::string error_;
};

bool saveScanReport(const FileScanResult& result,
                    const std::string& outputPath);

bool saveScanReportLocked(const FileScanResult& result,
                          const std::string& outputPath);

bool reconcileScanReportAfterPermanentDelete(
    const std::string& reportPath,
    const ArchiveDeletionResult& deletion,
    std::string& errorMessage
);

#ifdef SFV_ENABLE_TEST_HOOK
void setScanReportWriteTestHook(bool (*hook)());
void setScanReportLockBlockedTestHook(void (*hook)());
#endif

#endif
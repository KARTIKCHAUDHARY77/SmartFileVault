#ifndef SCAN_REPORT_H
#define SCAN_REPORT_H

#include "file_manager.h"
#include "archive_service.h"

#include <string>

// Exclusive, process-wide lock guarding read-modify-write access to one
// scan_report.json. The lock is a POSIX flock on the stable sidecar file
// "<reportPath>.lock". Scan publication and permanent-delete reconciliation
// must use the same report path so they contend on the same lock file.
//
// The lock file is intentionally never unlinked: removing it would let two
// parties lock different inodes for the same logical report.
//
// flock locks are per open file description, so acquiring this lock twice
// (even from one thread) deadlocks. Do not nest it; use saveScanReportLocked()
// while already holding a ScanReportLock.
class ScanReportLock {
public:
    explicit ScanReportLock(const std::string& reportPath);
    ~ScanReportLock();

    ScanReportLock(const ScanReportLock&) = delete;
    ScanReportLock& operator=(const ScanReportLock&) = delete;

    // True when the exclusive lock was acquired.
    bool locked() const { return fd_ >= 0; }

    // Describes why acquisition failed; empty when locked().
    const std::string& errorMessage() const { return error_; }

    // Deterministic lock-file path for a report path.
    static std::string lockPathFor(const std::string& reportPath);

private:
    int fd_;
    std::string error_;
};

// Acquires ScanReportLock, writes the report, then releases the lock.
bool saveScanReport(const FileScanResult& result,
                    const std::string& outputPath);

// Writes the report assuming the caller already holds the ScanReportLock for
// outputPath. Never acquires the lock itself.
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
#endif

#endif
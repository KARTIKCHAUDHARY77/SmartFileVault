#include "scan_report.h"

#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

namespace
{
    std::mutex lockWaitMutex;
    std::condition_variable lockWaitCondition;
    bool lockWaitObserved = false;

    void reportLockBlocked()
    {
        {
            std::lock_guard<std::mutex> guard(lockWaitMutex);
            lockWaitObserved = true;
        }
        lockWaitCondition.notify_one();
    }
    bool failReportWrite()
    {
        return true;
    }

    void require(bool condition, const std::string& message)
    {
        if (!condition) {
            std::cerr << "FAIL: " << message << "\n";
            std::exit(1);
        }
    }

    std::string readAll(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        std::ostringstream contents;
        contents << input.rdbuf();
        return contents.str();
    }

    void writeReport(const fs::path& path)
    {
        std::ofstream output(path, std::ios::binary);
        output << R"JSON({
  "totalFiles": 3,
  "inactiveFiles": 2,
  "activeFiles": 1,
  "skippedFiles": 2,
  "skippedDetails": [{"filePath":"/skipped/item","reason":"permission denied"}],
  "files": [
    {"fileName":"same.txt","filePath":"/folderA/same.txt","extension":".txt","size":1,"sizeText":"1 B","lastModified":1,"status":"inactive"},
    {"fileName":"same.txt","filePath":"/folderB/same.txt","extension":".txt","size":2,"sizeText":"2 B","lastModified":2,"status":"active"},
    {"fileName":"keep.txt","filePath":"/folderC/keep.txt","extension":".txt","size":3,"sizeText":"3 B","lastModified":3,"status":"inactive"}
  ]
})JSON";
        require(output.good(), "fixture report should be written");
    }

    ArchiveDeletionResult eligible(const std::string& path)
    {
        ArchiveDeletionResult result{};
        result.archiveDeleted = true;
        result.compressedRemoved = true;
        result.metadataRemoved = true;
        result.originalDeleted = true;
        result.originalPath = path;
        return result;
    }

    void expectNoRemoval(const fs::path& report, ArchiveDeletionResult result,
                         const std::string& label)
    {
        writeReport(report);
        const std::string before = readAll(report);
        std::string error;
        require(reconcileScanReportAfterPermanentDelete(
                    report.string(), result, error),
                label + " should be a safe no-op");
        require(readAll(report) == before, label + " must retain the Files report");
    }
}

int main()
{
    char pattern[] = "/tmp/sfv-scan-reconcile-XXXXXX";
    char* directory = ::mkdtemp(pattern);
    require(directory != nullptr, "temporary directory should be created");
    const fs::path root(directory);
    const fs::path report = root / "scan_report.json";

    // archiveDeleted does not mean the original or both artifacts were deleted.
    ArchiveDeletionResult result = eligible("/folderA/same.txt");
    result.archiveDeleted = false;
    expectNoRemoval(report, result, "archiveDeleted=false");
    result = eligible("/folderA/same.txt");
    result.compressedRemoved = false;
    expectNoRemoval(report, result, "compressed deletion failure");
    result = eligible("/folderA/same.txt");
    result.metadataRemoved = false;
    expectNoRemoval(report, result, "metadata deletion failure");
    result = eligible("/folderA/same.txt");
    result.originalDeleted = false;
    result.originalPreserved = true;
    result.errorCode = "original_delete_failed";
    expectNoRemoval(report, result, "original unlink failure");
    result = eligible("/folderA/same.txt");
    result.originalDeleted = false;
    result.originalPreserved = true;
    result.originalStatus = "preserved_identity_mismatch";
    expectNoRemoval(report, result, "modified or replacement original");

    // A missing original is stale and should be removed after artifact cleanup.
    writeReport(report);
    result = eligible("/folderA/same.txt");
    result.originalDeleted = false;
    result.originalMissing = true;
    std::string error;
    require(reconcileScanReportAfterPermanentDelete(
                report.string(), result, error),
            "missing original should reconcile");
    std::string updated = readAll(report);
    require(updated.find("/folderA/same.txt") == std::string::npos,
            "exact missing original path should be removed");
    require(updated.find("/folderB/same.txt") != std::string::npos,
            "same filename in another directory must remain");
    require(updated.find("\"totalFiles\": 2") != std::string::npos,
            "totalFiles should be recalculated");
    require(updated.find("\"inactiveFiles\": 1") != std::string::npos,
            "inactiveFiles should be recalculated");
    require(updated.find("\"activeFiles\": 1") != std::string::npos,
            "activeFiles should be recalculated");
    require(updated.find("\"skippedFiles\": 2") != std::string::npos,
            "skippedFiles must be preserved");
    require(updated.find("\"skippedDetails\"") != std::string::npos &&
            updated.find("permission denied") != std::string::npos,
            "skippedDetails must be preserved");

    // Exact path matching also applies when duplicate basenames are present.
    writeReport(report);
    result = eligible("/folderB/same.txt");
    error.clear();
    require(reconcileScanReportAfterPermanentDelete(
                report.string(), result, error),
            "duplicate-name path should reconcile");
    updated = readAll(report);
    require(updated.find("/folderA/same.txt") != std::string::npos &&
            updated.find("/folderB/same.txt") == std::string::npos,
            "only the exact duplicate path should be removed");

    // A persistence failure is surfaced and does not silently claim success.
    writeReport(report);
    const std::string beforeFailure = readAll(report);
    result = eligible("/folderA/same.txt");
    setScanReportWriteTestHook(failReportWrite);
    error.clear();
    const bool saved = reconcileScanReportAfterPermanentDelete(
        report.string(), result, error
    );
    setScanReportWriteTestHook(nullptr);
    require(!saved, "report persistence failure must be returned");
    require(!error.empty(), "report persistence failure must include an error");
    require(readAll(report) == beforeFailure,
            "failed report persistence must leave the original report intact");


    // A scan publishes its older snapshot while holding the report lock.
    // The hook confirms that deletion encountered actual OS-level lock contention.
    writeReport(report);
    {
        std::lock_guard<std::mutex> guard(lockWaitMutex);
        lockWaitObserved = false;
    }

    std::atomic<bool> deletionFinished{false};
    bool deletionSucceeded = false;
    std::string concurrentError;
    std::thread deletionThread;

    {
        ScanReportLock scanLock(report.string());
        require(scanLock.valid(), "scan should acquire report lock");

        setScanReportLockBlockedTestHook(reportLockBlocked);
        deletionThread = std::thread([&]() {
            ArchiveDeletionResult deleted = eligible("/folderA/same.txt");
            deletionSucceeded = reconcileScanReportAfterPermanentDelete(
                report.string(), deleted, concurrentError);
            deletionFinished.store(true);
        });

        {
            std::unique_lock<std::mutex> guard(lockWaitMutex);
            lockWaitCondition.wait(guard, []() { return lockWaitObserved; });
        }

        // Deletion is confirmed blocked; publish the stale snapshot before unlock.
        writeReport(report);
    }

    deletionThread.join();
    setScanReportLockBlockedTestHook(nullptr);

    require(deletionFinished.load(), "deletion reconciliation thread should finish");
    require(deletionSucceeded, "concurrent deletion reconciliation should succeed");
    updated = readAll(report);
    require(updated.find("/folderA/same.txt") == std::string::npos,
            "deleted path must not reappear after scan publication");
    require(updated.find("/folderB/same.txt") != std::string::npos &&
            updated.find("/folderC/keep.txt") != std::string::npos,
            "unrelated Files records must remain after concurrent reconciliation");
    std::cout << "PASS: scan publication and Delete Both reconciliation serialize under the report lock\n";

    fs::remove_all(root);
    std::cout << "PASS: archive/original deletion outcomes gate Files reconciliation\n";
    std::cout << "PASS: exact paths distinguish duplicate filenames\n";
    std::cout << "PASS: counts are recalculated and skipped details preserved\n";
    std::cout << "PASS: report-write failures are surfaced without replacing the report\n";
    return 0;
}

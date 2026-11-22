// Production-path concurrency regression for scan_report.json.
//
// Exercises the REAL runScanAndArchive() and the REAL
// reconcileScanReportAfterPermanentDelete(), not ScanReportLock in isolation.
//
// Scenario A (scan owns the lock):
//   A test hook pauses runScanAndArchive() after scanDirectory() succeeded and
//   before the snapshot is published, while it still holds the report lock.
//   The original file is deleted and reconciliation is started on another
//   thread. Reconciliation must block until the scan publishes. Without the
//   shared lock the stale snapshot would resurrect the deleted file.
//
// Scenario B (reconciliation owns the lock):
//   The scan-report write hook pauses reconciliation while it holds the lock.
//   A scan started meanwhile must block, then observe the missing original.
//
// Ordering is established with condition variables and bounded waits. The only
// "negative" waits (proving something has NOT happened yet) are short and are
// backed by a deterministic non-blocking flock() probe of the lock file.
//
// Build with -DSFV_ENABLE_TEST_HOOK (never in a production build).

#include "application_service.h"
#include "scan_report.h"

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace
{
    using Clock = std::chrono::steady_clock;

    const std::chrono::seconds kPositiveWait(15);
    const std::chrono::milliseconds kNegativeWait(400);

    std::string cleanupRoot;

    void cleanupAtExit()
    {
        if (!cleanupRoot.empty()) {
            std::error_code ignored;
            fs::remove_all(cleanupRoot, ignored);
        }
    }

    void fail(const std::string& message)
    {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);   // atexit removes the temporary directory
    }

    void require(bool condition, const std::string& message)
    {
        if (!condition) {
            fail(message);
        }
    }

    struct State {
        std::mutex mutex;
        std::condition_variable cv;

        bool hookPaused = false;
        bool hookReleased = false;

        bool reconcileStarted = false;
        bool reconcileDone = false;
        bool reconcileOk = false;
        std::string reconcileError;

        bool scanStarted = false;
        bool scanDone = false;
        bool scanOk = false;
    } state;

    void resetState()
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.hookPaused = false;
        state.hookReleased = false;
        state.reconcileStarted = false;
        state.reconcileDone = false;
        state.reconcileOk = false;
        state.reconcileError.clear();
        state.scanStarted = false;
        state.scanDone = false;
        state.scanOk = false;
    }

    void setFlag(bool State::*flag)
    {
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.*flag = true;
        }
        state.cv.notify_all();
    }

    bool waitFor(bool State::*flag, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(state.mutex);
        return state.cv.wait_for(lock, timeout,
                                 [&] { return state.*flag; });
    }

    // Pause the calling thread until released (bounded so it can never hang).
    void pauseUntilReleased()
    {
        std::unique_lock<std::mutex> lock(state.mutex);
        state.hookPaused = true;
        state.cv.notify_all();
        state.cv.wait_for(lock, kPositiveWait,
                          [] { return state.hookReleased; });
    }

    // Installed on runScanAndArchive(): runs while the scan owns the lock.
    void scanBeforePublishHook()
    {
        pauseUntilReleased();
    }

    // Installed on reconciliation: runs while reconciliation owns the lock,
    // just before the atomic replace. Returning false means "do not inject a
    // failure", so the write proceeds normally after the pause.
    bool reconcileBeforeWriteHook()
    {
        pauseUntilReleased();
        return false;
    }

    // Always release the pause and clear every hook, even when a check fails.
    struct HookGuard {
        ~HookGuard()
        {
            setScanBeforeReportPublishTestHook(nullptr);
            setScanReportWriteTestHook(nullptr);
            {
                std::lock_guard<std::mutex> lock(state.mutex);
                state.hookReleased = true;
            }
            state.cv.notify_all();
        }
    };

    void releaseHook()
    {
        setFlag(&State::hookReleased);
    }

    std::string readAll(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        std::ostringstream contents;
        contents << input.rdbuf();
        return contents.str();
    }

    void writeFile(const fs::path& path, const std::string& text)
    {
        std::ofstream output(path, std::ios::binary);
        output << text;
        require(output.good(), "fixture file should be written: " + path.string());
    }

    bool contains(const std::string& text, const std::string& needle)
    {
        return text.find(needle) != std::string::npos;
    }

    // True when another open file description cannot take the report lock now,
    // which proves some party currently owns it.
    bool lockIsHeldByAnotherOwner(const fs::path& report)
    {
        const std::string lockPath =
            ScanReportLock::lockPathFor(report.string());
        int fd = ::open(lockPath.c_str(), O_RDWR | O_CLOEXEC);
        require(fd >= 0, "lock file must exist at the shared lock path");
        const int rc = ::flock(fd, LOCK_EX | LOCK_NB);
        const int savedErrno = errno;
        if (rc == 0) {
            ::flock(fd, LOCK_UN);
        }
        ::close(fd);
        return rc != 0 && savedErrno == EWOULDBLOCK;
    }

    bool lockIsFree(const fs::path& report)
    {
        return !lockIsHeldByAnotherOwner(report);
    }

    ArchiveDeletionResult eligibleDeletion(const std::string& originalPath)
    {
        ArchiveDeletionResult result{};
        result.archiveDeleted = true;
        result.compressedRemoved = true;
        result.metadataRemoved = true;
        result.originalDeleted = true;
        result.originalPath = originalPath;
        return result;
    }

    struct Fixture {
        fs::path source;
        fs::path vault;
        fs::path report;
        fs::path keep;
        fs::path gone;
    };

    // Nothing is archived: a 36,500-day threshold keeps every file active.
    const int kInactiveDays = 36500;

    bool scan(const Fixture& f)
    {
        ApplicationResult result{};
        return runScanAndArchive(f.source.string(), kInactiveDays,
                                 f.vault.string(), result);
    }

    Fixture makeFixture(const fs::path& root, const std::string& name)
    {
        Fixture f;
        const fs::path base = root / name;
        f.source = base / "source";
        f.vault = base / "vault";
        f.report = f.vault / "scan_report.json";
        f.keep = f.source / "keep.txt";
        f.gone = f.source / "gone.txt";
        fs::create_directories(f.source);
        writeFile(f.keep, "keep me");
        writeFile(f.gone, "delete me");

        // Seed a report that lists both files (a previous scan).
        require(scan(f), name + ": seed scan should succeed");
        const std::string seeded = readAll(f.report);
        require(contains(seeded, f.keep.string()) &&
                contains(seeded, f.gone.string()),
                name + ": seed report should list both files");
        require(contains(seeded, "\"totalFiles\": 2"),
                name + ": seed report should count two files");
        require(fs::exists(ScanReportLock::lockPathFor(f.report.string())),
                name + ": scan must create the shared lock file");
        return f;
    }

    void verifyFinalReport(const Fixture& f, const std::string& label)
    {
        const std::string report = readAll(f.report);
        require(!report.empty(), label + ": final report must exist");
        if (contains(report, f.gone.string())) {
            std::cerr << "---- final report ----\n" << report
                      << "---- end report ----\n";
        }
        require(!contains(report, f.gone.string()),
                label + ": deleted file must not remain in the report");
        require(contains(report, f.keep.string()),
                label + ": unrelated file entry must remain");
        require(contains(report, "\"totalFiles\": 1"),
                label + ": totalFiles must be consistent");
        require(contains(report, "\"inactiveFiles\": 0"),
                label + ": inactiveFiles must be consistent");
        require(contains(report, "\"activeFiles\": 1"),
                label + ": activeFiles must be consistent");
        require(contains(report, "\"skippedDetails\""),
                label + ": skippedDetails must be preserved");

        // Structural sanity: balanced braces/brackets outside of strings and
        // a single top-level object.
        int depth = 0;
        bool inString = false;
        bool escaped = false;
        for (char ch : report) {
            if (inString) {
                if (escaped) escaped = false;
                else if (ch == '\\') escaped = true;
                else if (ch == '"') inString = false;
                continue;
            }
            if (ch == '"') inString = true;
            else if (ch == '{' || ch == '[') ++depth;
            else if (ch == '}' || ch == ']') --depth;
            require(depth >= 0, label + ": report nesting must not go negative");
        }
        require(depth == 0 && !inString,
                label + ": final report must be well-formed JSON");

        require(fs::exists(ScanReportLock::lockPathFor(f.report.string())),
                label + ": lock file must be left in place after release");
        require(lockIsFree(f.report),
                label + ": lock must be released when both operations finish");
    }

    void scenarioScanOwnsLock(const fs::path& root)
    {
        resetState();
        HookGuard guard;
        const Fixture f = makeFixture(root, "scan-first");

        setScanBeforeReportPublishTestHook(scanBeforePublishHook);

        // The real production entry point runs on its own thread and pauses
        // after scanDirectory() but before publication.
        std::thread scanThread([&] {
            const bool ok = scan(f);
            {
                std::lock_guard<std::mutex> lock(state.mutex);
                state.scanOk = ok;
            }
            setFlag(&State::scanDone);
        });

        require(waitFor(&State::hookPaused, kPositiveWait),
                "A: scan should reach the pre-publication hook");
        require(lockIsHeldByAnotherOwner(f.report),
                "A: runScanAndArchive must hold the report lock before "
                "publication");

        // The file is deleted after the scan already listed it.
        require(fs::remove(f.gone), "A: original should be deleted");

        std::thread reconcileThread([&] {
            setFlag(&State::reconcileStarted);
            std::string error;
            const bool ok = reconcileScanReportAfterPermanentDelete(
                f.report.string(), eligibleDeletion(f.gone.string()), error);
            {
                std::lock_guard<std::mutex> lock(state.mutex);
                state.reconcileOk = ok;
                state.reconcileError = error;
            }
            setFlag(&State::reconcileDone);
        });

        require(waitFor(&State::reconcileStarted, kPositiveWait),
                "A: reconciliation thread should start");
        require(!waitFor(&State::reconcileDone, kNegativeWait),
                "A: reconciliation must block while the scan owns the lock");
        require(!waitFor(&State::scanDone, std::chrono::milliseconds(0)),
                "A: scan must still be paused before publication");
        require(contains(readAll(f.report), f.gone.string()),
                "A: report must be untouched while the scan owns the lock");

        releaseHook();
        scanThread.join();

        require(waitFor(&State::reconcileDone, kPositiveWait),
                "A: reconciliation should complete after the scan releases");
        reconcileThread.join();

        {
            std::lock_guard<std::mutex> lock(state.mutex);
            require(state.scanOk, "A: scan should succeed");
            require(state.reconcileOk,
                    "A: reconciliation should succeed: " + state.reconcileError);
        }

        verifyFinalReport(f, "A");
    }

    void scenarioReconcileOwnsLock(const fs::path& root)
    {
        resetState();
        HookGuard guard;
        const Fixture f = makeFixture(root, "reconcile-first");

        require(fs::remove(f.gone), "B: original should be deleted");

        // Pause the real reconciliation while it holds the lock.
        setScanReportWriteTestHook(reconcileBeforeWriteHook);

        std::thread reconcileThread([&] {
            setFlag(&State::reconcileStarted);
            std::string error;
            const bool ok = reconcileScanReportAfterPermanentDelete(
                f.report.string(), eligibleDeletion(f.gone.string()), error);
            {
                std::lock_guard<std::mutex> lock(state.mutex);
                state.reconcileOk = ok;
                state.reconcileError = error;
            }
            setFlag(&State::reconcileDone);
        });

        require(waitFor(&State::hookPaused, kPositiveWait),
                "B: reconciliation should reach the pre-write hook");
        require(lockIsHeldByAnotherOwner(f.report),
                "B: reconciliation must hold the report lock while writing");

        std::thread scanThread([&] {
            setFlag(&State::scanStarted);
            const bool ok = scan(f);
            {
                std::lock_guard<std::mutex> lock(state.mutex);
                state.scanOk = ok;
            }
            setFlag(&State::scanDone);
        });

        require(waitFor(&State::scanStarted, kPositiveWait),
                "B: scan thread should start");
        require(!waitFor(&State::scanDone, kNegativeWait),
                "B: scan must block while reconciliation owns the lock");

        releaseHook();
        reconcileThread.join();
        require(waitFor(&State::scanDone, kPositiveWait),
                "B: scan should complete after reconciliation releases");
        scanThread.join();

        {
            std::lock_guard<std::mutex> lock(state.mutex);
            require(state.reconcileOk,
                    "B: reconciliation should succeed: " + state.reconcileError);
            require(state.scanOk, "B: scan should succeed");
        }

        verifyFinalReport(f, "B");
    }

    void scenarioStandaloneSaveUsesLock(const fs::path& root)
    {
        // Standalone saveScanReport() must acquire the shared lock, and the
        // locked variant must not (no recursive acquisition).
        const fs::path dir = root / "standalone";
        fs::create_directories(dir);
        const fs::path report = dir / "scan_report.json";
        FileScanResult empty;

        {
            ScanReportLock held(report.string());
            require(held.locked(), "standalone: lock should be acquired");
            // Writing through the locked variant while holding the lock must
            // complete (it must not try to lock again).
            require(saveScanReportLocked(empty, report.string()),
                    "standalone: locked save must not re-acquire the lock");
        }

        require(saveScanReport(empty, report.string()),
                "standalone: public save should lock and succeed");
        require(lockIsFree(report),
                "standalone: public save must release the lock");

        // Public save blocks while another owner holds the lock.
        bool blockedDone = false;
        std::mutex m;
        std::condition_variable cv;
        std::thread saver;
        {
            ScanReportLock held(report.string());
            require(held.locked(), "standalone: lock should be acquired");
            saver = std::thread([&] {
                FileScanResult data;
                saveScanReport(data, report.string());
                {
                    std::lock_guard<std::mutex> lock(m);
                    blockedDone = true;
                }
                cv.notify_all();
            });
            std::unique_lock<std::mutex> lock(m);
            require(!cv.wait_for(lock, kNegativeWait,
                                 [&] { return blockedDone; }),
                    "standalone: public save must wait for the shared lock");
        }
        {
            std::unique_lock<std::mutex> lock(m);
            require(cv.wait_for(lock, kPositiveWait,
                                [&] { return blockedDone; }),
                    "standalone: public save should finish after release");
        }
        saver.join();
    }
}

int main()
{
    char pattern[] = "/tmp/sfv-report-concurrency-XXXXXX";
    char* directory = ::mkdtemp(pattern);
    require(directory != nullptr, "temporary directory should be created");
    cleanupRoot = directory;
    std::atexit(cleanupAtExit);
    // The scanner records canonical paths (macOS /tmp -> /private/tmp), and
    // reconciliation matches exact paths, so the fixture must use them too.
    const fs::path root = fs::canonical(directory);

    scenarioStandaloneSaveUsesLock(root);
    std::cout << "PASS: standalone save locks; locked save never re-locks\n";

    scenarioScanOwnsLock(root);
    std::cout << "PASS: reconciliation blocks while runScanAndArchive owns the "
                 "report lock; deleted file is absent from the final report\n";

    scenarioReconcileOwnsLock(root);
    std::cout << "PASS: scan blocks while reconciliation owns the report lock; "
                 "final report is consistent\n";

    return 0;
}

#include "compression.h"
#include "test_support.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

// Test-only function defined in compression.cpp
// (only exists when built with -DSFV_ENABLE_TEST_HOOK).
void setCompressionTestHook(void (*hook)());

static fs::path sourcePath;
static fs::file_time_type originalTime;
static bool modificationSucceeded = false;

static const std::string originalContent = "AAAAAAAAAAAAAAAAAAAA";
static const std::string modifiedContent = "BBBBBBBBBBBBBBBBBBBB";

// Guarantees the hook is cleared even if compressFile throws.
struct HookGuard {
    ~HookGuard() { setCompressionTestHook(nullptr); }
};

static void modifySourceFile() {
    const std::vector<unsigned char> bytes(
        modifiedContent.begin(), modifiedContent.end());

    if (!testsupport::writeAllBytes(sourcePath, bytes)) {
        return;
    }

    // Restore the old timestamp so size and mtime checks alone
    // cannot detect the change; only hashing can.
    std::error_code error;
    fs::last_write_time(sourcePath, originalTime, error);

    modificationSucceeded = !error;
}

int main() {
    testsupport::TestDirectory dir(
        "smartfilevault_source_change_test");

    if (!dir.ok()) {
        std::cerr << "FAIL: Could not create test directory\n";
        return 1;
    }

    int failures = 0;
    std::error_code error;

    // Control: with no hook, an unchanged file must compress fine.
    // This proves the rejection below is caused by the source change.
    {
        const fs::path controlSource = dir.path() / "control.txt";
        const fs::path controlOutput = dir.path() / "control.gz";

        const std::vector<unsigned char> bytes(
            originalContent.begin(), originalContent.end());

        if (!testsupport::writeAllBytes(controlSource, bytes)) {
            std::cerr << "FAIL: Could not create control file\n";
            return 1;
        }

        const bool ok = compressFile(
            controlSource.string(), controlOutput.string());

        const bool exists = fs::exists(controlOutput, error);

        if (ok && exists && !error) {
            std::cout << "PASS: Control - unchanged source compressed\n";
        } else {
            std::cerr << "FAIL: Control - unchanged source was not "
                         "compressed; results below are not trustworthy\n";
            ++failures;
        }
    }

    sourcePath = dir.path() / "source.txt";
    const fs::path compressedPath = dir.path() / "source.gz";

    {
        const std::vector<unsigned char> bytes(
            originalContent.begin(), originalContent.end());

        if (!testsupport::writeAllBytes(sourcePath, bytes)) {
            std::cerr << "FAIL: Could not create source file\n";
            return 1;
        }
    }

    originalTime = fs::last_write_time(sourcePath, error);

    if (error) {
        std::cerr << "FAIL: Could not read original timestamp\n";
        return 1;
    }

    bool compressionSucceeded = true;

    {
        HookGuard guard;
        setCompressionTestHook(modifySourceFile);

        compressionSucceeded = compressFile(
            sourcePath.string(), compressedPath.string());
    }

    if (modificationSucceeded) {
        std::cout << "PASS: Source modified during compression\n";
    } else {
        std::cerr << "FAIL: Source modification did not complete\n";
        ++failures;
    }

    error.clear();
    const auto finalSize = fs::file_size(sourcePath, error);

    if (!error && finalSize == originalContent.size()) {
        std::cout << "PASS: Source size remained unchanged\n";
    } else {
        std::cerr << "FAIL: Source size changed unexpectedly\n";
        ++failures;
    }

    error.clear();
    const auto finalTime = fs::last_write_time(sourcePath, error);

    if (!error && finalTime == originalTime) {
        std::cout << "PASS: Source timestamp remained unchanged\n";
    } else {
        std::cerr << "FAIL: Source timestamp changed\n";
        ++failures;
    }

    std::vector<unsigned char> actualBytes;
    const std::vector<unsigned char> expectedBytes(
        modifiedContent.begin(), modifiedContent.end());

    if (testsupport::readAllBytes(sourcePath, actualBytes) &&
        actualBytes == expectedBytes) {
        std::cout << "PASS: Modified source content confirmed\n";
    } else {
        std::cerr << "FAIL: Modified source content not found\n";
        ++failures;
    }

    if (!compressionSucceeded) {
        std::cout << "PASS: Compression rejected changed source\n";
    } else {
        std::cerr << "FAIL: Compression accepted changed source\n";
        ++failures;
    }

    error.clear();
    const bool outputExists = fs::exists(compressedPath, error);

    if (!error && !outputExists) {
        std::cout << "PASS: Failed compressed output removed\n";
    } else {
        std::cerr << "FAIL: Compressed output was retained\n";
        ++failures;
    }

    if (failures != 0) {
        std::cerr << "Source-change tests failed: "
                  << failures << '\n';
        return 1;
    }

    if (!dir.cleanup()) {
        return 1;
    }

    std::cout << "All source-change tests passed.\n";
    return 0;
}
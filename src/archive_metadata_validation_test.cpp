#include "archive.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

struct TempCleanup {
    fs::path root;

    ~TempCleanup() {
        std::error_code error;
        fs::remove_all(root, error);

        if (error) {
            std::cerr
                << "WARNING: Temporary cleanup failed: "
                << error.message() << '\n';
        }
    }
};

static bool writeFixture(
    const fs::path& path,
    const std::string& originalSize,
    const std::string& compressedSize,
    const std::string& percentage
) {
    std::ofstream output(path, std::ios::binary);

    if (!output) {
        return false;
    }

    output << "TEST-001\n"
           << "sample.bin\n"
           << "/original/sample.bin\n"
           << "/archive/sample.bin.gz\n"
           << originalSize << '\n'
           << compressedSize << '\n'
           << percentage << '\n'
           << "12345\n";

    output.close();
    return !output.fail();
}

static bool checkCase(
    const fs::path& root,
    const std::string& name,
    const std::string& originalSize,
    const std::string& compressedSize,
    const std::string& percentage,
    bool expectedAccepted
) {
    const fs::path path = root / (name + ".meta");

    if (!writeFixture(
            path, originalSize, compressedSize, percentage)) {
        std::cerr << "FAIL: Could not write " << name << '\n';
        return false;
    }

    ArchiveMetadata metadata{};
    const bool accepted = loadArchiveMetadata(path.string(), metadata);

    if (accepted != expectedAccepted) {
        std::cerr << "FAIL: " << name
                  << " expected "
                  << (expectedAccepted ? "acceptance" : "rejection")
                  << ", got "
                  << (accepted ? "acceptance" : "rejection")
                  << '\n';
        return false;
    }

    std::cout << "PASS: " << name << '\n';
    return true;
}

int main() {
    const auto timestamp =
        std::chrono::steady_clock::now()
            .time_since_epoch().count();

    const fs::path root =
        fs::temp_directory_path() /
        ("sfv_metadata_validation_" + std::to_string(timestamp));

    std::error_code error;
    if (!fs::create_directory(root, error) || error) {
        std::cerr << "FAIL: Could not create test directory: "
                  << error.message() << '\n';
        return 1;
    }

    TempCleanup cleanup{root};
    bool passed = true;

    passed = checkCase(root, "valid",
        "1024", "512", "50.0", true) && passed;

    passed = checkCase(root, "negative-original-size",
        "-1", "512", "50.0", false) && passed;

    passed = checkCase(root, "negative-compressed-size",
        "1024", "-1", "50.0", false) && passed;

    passed = checkCase(root, "malformed-size",
        "1024bytes", "512", "50.0", false) && passed;

    passed = checkCase(root, "overflowing-size",
        "999999999999999999999999999999999999",
        "512", "50.0", false) && passed;

    passed = checkCase(root, "nan-percentage",
        "1024", "512", "nan", false) && passed;

    passed = checkCase(root, "infinite-percentage",
        "1024", "512", "inf", false) && passed;

    passed = checkCase(root, "negative-percentage",
        "1024", "512", "-1.0", false) && passed;

    passed = checkCase(root, "percentage-over-100",
        "1024", "512", "100.01", false) && passed;

    passed = checkCase(root, "trailing-percentage-data",
        "1024", "512", "50.0extra", false) && passed;

    if (!passed) {
        std::cerr << "ARCHIVE METADATA VALIDATION TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL ARCHIVE METADATA VALIDATION TESTS PASSED\n";
    return 0;
}

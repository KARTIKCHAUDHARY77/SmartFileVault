
#include "archive_service.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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
                << "WARNING: Could not clean temporary fixtures: "
                << error.message() << '\n';
        }
    }
};

static bool writeBinary(
    const fs::path& path,
    const std::string& content
) {
    std::ofstream file(path, std::ios::binary);

    if (!file) {
        return false;
    }

    file.write(
        content.data(),
        static_cast<std::streamsize>(content.size())
    );

    file.close();
    return !file.fail();
}

static std::string readBinary(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        return {};
    }

    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

int main() {
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    const fs::path root =
        fs::temp_directory_path() /
        ("sfv_archive_cleanup_" + std::to_string(stamp));

    TempCleanup cleanup{root};

    std::error_code error;

    // Prepare a unique test directory.
    fs::create_directories(root / "archive", error);

    if (error) {
        std::cerr
            << "FAIL: Cannot create test directory: "
            << error.message() << '\n';
        return 1;
    }

    // Repeating binary byte values makes the input highly
    // compressible while exercising null and non-text bytes.
    std::string expected;
    expected.reserve(512 * 1024);

    for (int i = 0; i < 512 * 1024; ++i) {
        expected.push_back(
            static_cast<char>(i % 256)
        );
    }

    bool passed = true;

    auto check = [&](bool condition, const char* message) {
        std::cout
            << (condition ? "PASS: " : "FAIL: ")
            << message << '\n';

        if (!condition) {
            passed = false;
        }
    };

    // ========================================================
    // CASE 1: Failure during verification setup
    // ========================================================

    const fs::path archiveRoot = root / "archive";
    const fs::path source = root / "payload.bin";
    const fs::path blocker = archiveRoot / "verify";

    if (!writeBinary(source, expected)) {
        std::cerr << "FAIL: Could not write original fixture.\n";
        return 1;
    }

    const std::string blockerContent =
        "KEEP THIS PRE-EXISTING FILE\n";

    if (!writeBinary(blocker, blockerContent)) {
        std::cerr
            << "FAIL: Could not create verification blocker.\n";
        return 1;
    }

    ArchiveMetadata metadata{};

    const bool archived = archiveAndRemoveOriginal(
        source.string(),
        archiveRoot.string(),
        metadata
    );

    check(
        !archived,
        "Invalid verification path rejects archiving."
    );

    check(
        !metadata.archiveId.empty() &&
        !metadata.archivePath.empty(),
        "Archive creation reached the verification stage."
    );

    check(
        fs::is_regular_file(source) &&
        readBinary(source) == expected,
        "Original binary contents remain unchanged."
    );

    check(
        fs::is_regular_file(blocker) &&
        readBinary(blocker) == blockerContent,
        "Pre-existing verification blocker remains unchanged."
    );

    if (!metadata.archiveId.empty()) {
        const fs::path metadataPath =
            archiveRoot / "metadata" /
            (metadata.archiveId + ".meta");

        check(
            !fs::exists(metadataPath),
            "Failed archive metadata is removed."
        );
    } else {
        check(
            false,
            "Failed archive metadata is removed."
        );
    }

    if (!metadata.archivePath.empty()) {
        check(
            !fs::exists(metadata.archivePath),
            "Failed compressed archive is removed."
        );
    } else {
        check(
            false,
            "Failed compressed archive is removed."
        );
    }

    // ========================================================
    // CASE 2: Successful archiving with an existing verify dir
    // ========================================================

    const fs::path successRoot = root / "success";
    const fs::path successArchiveRoot =
        successRoot / "archive";
    const fs::path successVerify =
        successArchiveRoot / "verify";
    const fs::path successSource =
        successRoot / "payload.bin";

    error.clear();

    fs::create_directories(
        successVerify,
        error
    );

    if (error) {
        std::cerr
            << "FAIL: Cannot create successful-archive fixture: "
            << error.message() << '\n';
        return 1;
    }

    if (!writeBinary(successSource, expected)) {
        std::cerr
            << "FAIL: Could not write successful-archive source.\n";
        return 1;
    }

    ArchiveMetadata successMetadata{};

    const bool success = archiveAndRemoveOriginal(
        successSource.string(),
        successArchiveRoot.string(),
        successMetadata
    );

    check(
        success,
        "Valid archive completes successfully."
    );

    check(
        !fs::exists(successSource),
        "Verified original is removed after successful archiving."
    );

    check(
        fs::is_directory(successVerify),
        "Pre-existing verification directory is preserved."
    );

    // ========================================================
    // FINAL RESULT
    // ========================================================

    if (passed) {
        std::cout << "ALL ARCHIVE CLEANUP TESTS PASSED\n";
        return 0;
    }

    std::cerr << "ARCHIVE CLEANUP REGRESSION FAILED\n";
    return 1;
}

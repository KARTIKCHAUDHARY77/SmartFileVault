#include "archive.h"
#include "restore_safety.h"

#include <filesystem>
#include <iostream>
#include <limits>

namespace fs = std::filesystem;

int main()
{
    const std::string archiveFolder =
        "restore_safety_test_output";

    fs::create_directories(
        archiveFolder + "/metadata"
    );

    ArchiveMetadata first;

    first.archiveId = "TEST-001";
    first.originalName = "file1.txt";
    first.originalPath = "/test/file1.txt";
    first.archivePath = "/test/archive1.gz";
    first.originalSize = 40 * 1024 * 1024;
    first.compressedSize = 10 * 1024 * 1024;
    first.compressionPercentage = 75.0;
    first.archivedAt = 1000;

    ArchiveMetadata second;

    second.archiveId = "TEST-002";
    second.originalName = "file2.txt";
    second.originalPath = "/test/file2.txt";
    second.archivePath = "/test/archive2.gz";
    second.originalSize = 150 * 1024 * 1024;
    second.compressedSize = 35 * 1024 * 1024;
    second.compressionPercentage = 76.6;
    second.archivedAt = 2000;

    saveArchiveMetadata(
        archiveFolder + "/metadata/TEST-001.meta",
        first
    );

    saveArchiveMetadata(
        archiveFolder + "/metadata/TEST-002.meta",
        second
    );

    std::uintmax_t safetySpace =
        getRestoreSafetySpace(
            archiveFolder
        );

    std::cout
        << "Restore safety space: "
        << safetySpace / (1024 * 1024)
        << " MB\n";

    std::cout
        << "Expected safety space: 150 MB\n";

    if (safetySpace ==
        150ULL * 1024ULL * 1024ULL) {

        std::cout
            << "Safety space calculation: PASS\n";
    }
    else {

        std::cout
            << "Safety space calculation: FAIL\n";
    }

    RestoreStorageStatus status =
        checkRestoreSpace(
            second,
            "."
        );

    if (!status.spaceCheckSucceeded) {
        std::cerr
            << "FAIL: Free-space lookup failed for existing directory.\n";
        fs::remove_all(archiveFolder);
        return 1;
    }

    std::cout
        << "Current free space: "
        << status.freeSpace /
           (1024 * 1024)
        << " MB\n";

    std::cout
        << "Required for selected file: "
        << status.requiredSpace /
           (1024 * 1024)
        << " MB\n";

    std::cout
        << "Enough space: "
        << (status.enoughSpace
                ? "YES"
                : "NO")
        << '\n';
    // A missing child path must not masquerade as measured zero space.
    RestoreStorageStatus missingStatus =
        checkRestoreSpace(
            second,
            archiveFolder + "/missing-destination"
        );

    if (missingStatus.spaceCheckSucceeded ||
        missingStatus.enoughSpace) {
        std::cerr
            << "FAIL: Missing destination passed the space preflight.\n";
        fs::remove_all(archiveFolder);
        return 1;
    }

    std::cout
        << "Missing destination lookup rejected: PASS\n";

    // Test that an impossible restore size is rejected safely.
    ArchiveMetadata impossible = second;

    impossible.originalSize =
        std::numeric_limits<decltype(impossible.originalSize)>::max();

    RestoreStorageStatus insufficient =
        checkRestoreSpace(
            impossible,
            archiveFolder
        );

    if (insufficient.freeSpace == 0) {
        std::cerr
            << "FAIL: Could not determine free space "
            << "for the test directory.\n";

        fs::remove_all(archiveFolder);
        return 1;
    }

    if (insufficient.enoughSpace) {
        std::cerr
            << "FAIL: Insufficient-space request was accepted.\n";

        fs::remove_all(archiveFolder);
        return 1;
    }

    std::cout << "Insufficient-space rejection: PASS\n";

    // Keep your existing cleanup below this line.
    fs::remove_all(
        archiveFolder
    );

    return 0;
}
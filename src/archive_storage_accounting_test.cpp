#include "archive.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

struct Cleanup {
    fs::path root;

    ~Cleanup() {
        std::error_code error;
        fs::remove_all(root, error);
        if (error) {
            std::cerr << "WARNING: cleanup failed: "
                      << error.message() << '\n';
        }
    }
};

static bool writeFixture(
    const fs::path& root,
    const std::string& id,
    std::uintmax_t original,
    std::uintmax_t compressed
)
{
    std::error_code error;
    const fs::path metadataDir = root / "metadata";

    fs::create_directories(metadataDir, error);
    if (error || !fs::is_directory(metadataDir)) {
        return false;
    }

    ArchiveMetadata metadata{};
    metadata.archiveId = id;
    metadata.originalName = id + ".bin";
    metadata.originalPath = "/original/" + id;
    metadata.archivePath = "/compressed/" + id + ".gz";
    metadata.originalSize = original;
    metadata.compressedSize = compressed;
    metadata.compressionPercentage = 50.0;
    metadata.archivedAt = 12345;

    return saveArchiveMetadata(
        (metadataDir / (id + ".meta")).string(), metadata
    );
}

static bool check(bool result, const char* name)
{
    std::cout << (result ? "PASS: " : "FAIL: ")
              << name << '\n';
    return result;
}

int main()
{
    const auto tick =
        std::chrono::steady_clock::now()
            .time_since_epoch().count();

    const fs::path root =
        fs::temp_directory_path() /
        ("sfv_accounting_" + std::to_string(tick));

    Cleanup cleanup{root};
    bool passed = true;

    const fs::path normal = root / "normal";

    if (!writeFixture(normal, "one", 100, 50) ||
        !writeFixture(normal, "two", 200, 110)) {
        std::cerr << "FAIL: Could not create normal fixtures.\n";
        return 1;
    }

    std::uintmax_t original = 0;
    std::uintmax_t compressed = 0;
    std::uintmax_t saved = 0;

    passed = check(
        tryGetTotalOriginalSize(normal.string(), original) &&
        original == 300 &&
        tryGetTotalCompressedSize(normal.string(), compressed) &&
        compressed == 160 &&
        tryGetTotalSpaceSaved(normal.string(), saved) &&
        saved == 140,
        "Normal totals and storage savings"
    ) && passed;

    const fs::path originalOverflow = root / "original-overflow";

    if (!writeFixture(
            originalOverflow, "max",
            std::numeric_limits<std::uintmax_t>::max(), 1) ||
        !writeFixture(originalOverflow, "extra", 1, 1)) {
        std::cerr << "FAIL: Could not create original overflow fixtures.\n";
        return 1;
    }

    original = 99;
    saved = 99;

    passed = check(
        !tryGetTotalOriginalSize(
            originalOverflow.string(), original) &&
        original == 0 &&
        getTotalOriginalSize(originalOverflow.string()) ==
            std::numeric_limits<std::uintmax_t>::max() &&
        !tryGetTotalSpaceSaved(originalOverflow.string(), saved) &&
        saved == 0 &&
        getTotalSpaceSaved(originalOverflow.string()) == 0,
        "Original-size overflow detected"
    ) && passed;

    const fs::path compressedOverflow = root / "compressed-overflow";

    if (!writeFixture(
            compressedOverflow, "max",
            1, std::numeric_limits<std::uintmax_t>::max()) ||
        !writeFixture(compressedOverflow, "extra", 1, 1)) {
        std::cerr << "FAIL: Could not create compressed overflow fixtures.\n";
        return 1;
    }

    compressed = 99;
    saved = 99;

    passed = check(
        !tryGetTotalCompressedSize(
            compressedOverflow.string(), compressed) &&
        compressed == 0 &&
        getTotalCompressedSize(compressedOverflow.string()) ==
            std::numeric_limits<std::uintmax_t>::max() &&
        !tryGetTotalSpaceSaved(compressedOverflow.string(), saved) &&
        saved == 0 &&
        getTotalSpaceSaved(compressedOverflow.string()) == 0,
        "Compressed-size overflow detected"
    ) && passed;

    if (!passed) {
        std::cerr << "STORAGE ACCOUNTING TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL STORAGE ACCOUNTING TESTS PASSED\n";
    return 0;
}

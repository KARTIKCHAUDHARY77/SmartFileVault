#include "vault_service.h"
#include "vault_report.h"

#include <chrono>
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
    std::uintmax_t originalSize,
    std::uintmax_t compressedSize
)
{
    std::error_code error;
    const fs::path metadataDirectory = root / "metadata";

    fs::create_directories(metadataDirectory, error);

    if (error || !fs::is_directory(metadataDirectory)) {
        return false;
    }

    ArchiveMetadata metadata{};
    metadata.archiveId = id;
    metadata.originalName = id + ".bin";
    metadata.originalPath =
        (root / "original" / metadata.originalName).string();
    metadata.archivePath =
        (root / "archives" / (id + ".gz")).string();
    metadata.originalSize = originalSize;
    metadata.compressedSize = compressedSize;
    metadata.compressionPercentage = 50.0;
    metadata.archivedAt = 12345;

    return saveArchiveMetadata(
        (metadataDirectory / (id + ".meta")).string(),
        metadata
    );
}

static bool check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS: " : "FAIL: ")
              << name << '\n';
    return condition;
}

static bool summaryIsZero(const VaultSummary& summary)
{
    return summary.archiveCount == 0 &&
           summary.originalSize == 0 &&
           summary.compressedSize == 0 &&
           summary.spaceSaved == 0 &&
           summary.restoreSafetySpace == 0 &&
           summary.freeSpace == 0;
}

int main()
{
    const auto tick =
        std::chrono::steady_clock::now()
            .time_since_epoch().count();

    const fs::path root =
        fs::temp_directory_path() /
        ("sfv_vault_summary_" + std::to_string(tick));

    Cleanup cleanup{root};
    bool passed = true;

    const fs::path normal = root / "normal";

    if (!writeFixture(normal, "one", 100, 50) ||
        !writeFixture(normal, "two", 200, 110)) {
        std::cerr << "FAIL: Could not create normal metadata fixtures.\n";
        return 1;
    }

    VaultSummary summary{};

    const bool normalResult =
        getVaultSummary(normal.string(), summary);

    passed = check(
        normalResult &&
        summary.archiveCount == 2 &&
        summary.originalSize == 300 &&
        summary.compressedSize == 160 &&
        summary.spaceSaved == 140 &&
        summary.restoreSafetySpace == 200 &&
        summary.freeSpace > 0,
        "Normal vault summary totals"
    ) && passed;

    const fs::path originalOverflow = root / "original-overflow";

    if (!writeFixture(
            originalOverflow,
            "maximum",
            std::numeric_limits<std::uintmax_t>::max(),
            1
        ) ||
        !writeFixture(originalOverflow, "extra", 1, 1)) {
        std::cerr << "FAIL: Could not create original-overflow fixtures.\n";
        return 1;
    }

    summary = VaultSummary{99, 99, 99, 99, 99, 99};

    const bool originalOverflowResult =
        getVaultSummary(originalOverflow.string(), summary);

    const fs::path reportPath = root / "overflow-report.json";
    const bool reportRejected =
        !saveVaultReport(originalOverflow.string(), reportPath.string()) &&
        !fs::exists(reportPath);

    passed = check(
        !originalOverflowResult &&
        summaryIsZero(summary) &&
        reportRejected,
        "Original-size overflow rejects summary/report and clears all totals"
    ) && passed;

    const fs::path compressedOverflow = root / "compressed-overflow";

    if (!writeFixture(compressedOverflow, "one", 1, 1) ||
        !writeFixture(
            compressedOverflow,
            "maximum",
            1,
            std::numeric_limits<std::uintmax_t>::max()
        )) {
        std::cerr << "FAIL: Could not create compressed-overflow fixtures.\n";
        return 1;
    }

    summary = VaultSummary{99, 99, 99, 99, 99, 99};

    const bool compressedOverflowResult =
        getVaultSummary(compressedOverflow.string(), summary);

    passed = check(
        !compressedOverflowResult && summaryIsZero(summary),
        "Compressed-size overflow rejects summary and clears all totals"
    ) && passed;

    if (!passed) {
        std::cerr << "VAULT SUMMARY OVERFLOW TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL VAULT SUMMARY OVERFLOW TESTS PASSED\n";
    return 0;
}

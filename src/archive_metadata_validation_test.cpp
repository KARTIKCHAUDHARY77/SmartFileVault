#include "archive.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

struct MetadataCase {
    std::string name;
    std::string originalSize;
    std::string compressedSize;
    std::string percentage;
    std::string timestamp = "1730000000";
    std::string permissions;
    bool hasPermissions = false;
    bool crlf = false;
    bool expectedAccepted = false;
    bool checkLegacyFallback = false;
    bool checkPermissionValue = false;

    MetadataCase(
        std::string caseName,
        std::string original,
        std::string compressed,
        std::string percent,
        std::string archivedAt = "1730000000",
        std::string permissionText = "",
        bool hasPermissionLine = false,
        bool useCrlf = false,
        bool shouldAccept = false,
        bool verifyLegacyFallback = false,
        bool verifyPermissionValue = false
    )
        : name(caseName),
          originalSize(original),
          compressedSize(compressed),
          percentage(percent),
          timestamp(archivedAt),
          permissions(permissionText),
          hasPermissions(hasPermissionLine),
          crlf(useCrlf),
          expectedAccepted(shouldAccept),
          checkLegacyFallback(verifyLegacyFallback),
          checkPermissionValue(verifyPermissionValue)
    {
    }
};

struct TempCleanup {
    fs::path root;

    ~TempCleanup() {
        std::error_code error;
        fs::remove_all(root, error);
        if (error) {
            std::cerr << "WARNING: test cleanup failed: "
                      << error.message() << '\n';
        }
    }
};

static bool writeFixture(const fs::path& path, const MetadataCase& test)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }

    const char* newline = test.crlf ? "\r\n" : "\n";
    const std::string fields[] = {
        "SFV-METADATA-TEST",
        "sample.txt",
        "/original/sample.txt",
        "/archive/sample.txt.gz",
        test.originalSize,
        test.compressedSize,
        test.percentage,
        test.timestamp
    };

    for (const std::string& field : fields) {
        output << field << newline;
    }

    if (test.hasPermissions) {
        output << test.permissions << newline;
    }

    output.close();
    return !output.fail();
}

static bool runCase(const fs::path& root, const MetadataCase& test)
{
    const fs::path path = root / (test.name + ".meta");
    if (!writeFixture(path, test)) {
        std::cerr << "FAIL: could not write fixture: " << test.name << '\n';
        return false;
    }

    ArchiveMetadata metadata {};
    const bool accepted = loadArchiveMetadata(path.string(), metadata);
    if (accepted != test.expectedAccepted) {
        std::cerr << "FAIL: " << test.name
                  << " expected "
                  << (test.expectedAccepted ? "acceptance" : "rejection")
                  << ", got "
                  << (accepted ? "acceptance" : "rejection") << '\n';
        return false;
    }

    // Verify that accepted metadata contains the exact parsed values.
    if (accepted) {
        try {
            const auto expectedOriginalSize =
                static_cast<std::uintmax_t>(
                    std::stoull(test.originalSize)
                );

            const auto expectedCompressedSize =
                static_cast<std::uintmax_t>(
                    std::stoull(test.compressedSize)
                );

            const double expectedPercentage =
                std::stod(test.percentage);

            const long long expectedTimestamp =
                std::stoll(test.timestamp);

            if (metadata.originalSize != expectedOriginalSize ||
                metadata.compressedSize != expectedCompressedSize ||
                metadata.compressionPercentage != expectedPercentage ||
                metadata.archivedAt != expectedTimestamp) {
                std::cerr
                    << "FAIL: parsed metadata values do not match fixture: "
                    << test.name << '\n';
                return false;
            }
        } catch (const std::exception& error) {
            std::cerr
                << "FAIL: expected-value conversion failed for "
                << test.name << ": " << error.what() << '\n';
            return false;
        }
    }

    if (accepted && test.checkLegacyFallback &&
        (metadata.hasOriginalPermissions || metadata.originalPermissions != 0600U)) {
        std::cerr << "FAIL: legacy permission fallback: " << test.name << '\n';
        return false;
    }

    if (accepted && test.checkPermissionValue &&
        (!metadata.hasOriginalPermissions || metadata.originalPermissions != 0644U)) {
        std::cerr << "FAIL: permission metadata value: " << test.name << '\n';
        return false;
    }

    std::cout << "PASS: " << test.name << '\n';
    return true;
}

int main()
{
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("sfv_metadata_validation_" + std::to_string(stamp));

    std::error_code error;
    if (!fs::create_directory(root, error) || error) {
        std::cerr << "FAIL: could not create test directory: "
                  << error.message() << '\n';
        return 1;
    }
    TempCleanup cleanup {root};

    std::ostringstream maximumStream;
    maximumStream << std::numeric_limits<std::uintmax_t>::max();
    const std::string maximum = maximumStream.str();
    const std::string overflow = maximum + "0";

    const std::string maximumTimestamp =
        std::to_string(std::numeric_limits<long long>::max());

    const std::string timestampOverflow =
        std::to_string(
            static_cast<unsigned long long>(
                std::numeric_limits<long long>::max()
            ) + 1ULL
        );

    std::vector<MetadataCase> cases = {
        {"valid-legacy-eight-line", "1024", "512", "50.0",
         "1730000000", "", false, false, true, true, false},
        {"valid-modern-permissions", "1024", "512", "50.0",
         "1730000000", "420", true, false, true, false, true},
        {"valid-crlf-legacy", "1024", "512", "50.0",
         "1730000000", "", false, true, true, true, false},
        {"valid-zero-percentage", "1024", "1024", "0",
         "1730000000", "", false, false, true, false, false},
        {"valid-one-hundred-percentage", "1024", "1", "100.0",
         "1730000000", "", false, false, true, false, false},
        {"valid-maximum-original-size", maximum, "1", "100.0",
         "1730000000", "", false, false, true, false, false},
        {"valid-maximum-compressed-size", "1", maximum, "0",
         "1730000000", "", false, false, true, false, false},

        {"negative-original-size", "-1", "3", "50.0"},
        {"plus-original-size", "+5", "3", "50.0"},
        {"leading-space-original-size", " 5", "3", "50.0"},
        {"trailing-space-original-size", "5 ", "3", "50.0"},
        {"leading-tab-original-size", "\t5", "3", "50.0"},
        {"trailing-tab-original-size", "5\t", "3", "50.0"},
        {"empty-original-size", "", "3", "50.0"},
        {"malformed-original-size", "5bytes", "3", "50.0"},
        {"overflow-original-size", overflow, "3", "50.0"},

        {"negative-compressed-size", "5", "-1", "50.0"},
        {"plus-compressed-size", "5", "+3", "50.0"},
        {"leading-space-compressed-size", "5", " 3", "50.0"},
        {"trailing-space-compressed-size", "5", "3 ", "50.0"},
        {"leading-tab-compressed-size", "5", "\t3", "50.0"},
        {"empty-compressed-size", "5", "", "50.0"},
        {"malformed-compressed-size", "5", "3bytes", "50.0"},
        {"overflow-compressed-size", "5", overflow, "50.0"},

        {"valid-maximum-timestamp", "5", "3", "50.0",
         maximumTimestamp, "", false, false, true, false, false},
        {"timestamp-over-long-long-max", "5", "3", "50.0",
         timestampOverflow},
        {"negative-timestamp", "5", "3", "50.0", "-1"},
        {"signed-timestamp", "5", "3", "50.0", "+123"},
        {"spaced-timestamp", "5", "3", "50.0", " 123"},
        {"overflow-timestamp", "5", "3", "50.0", overflow},

        {"nan-percentage", "5", "3", "nan"},
        {"infinite-percentage", "5", "3", "inf"},
        {"negative-infinity-percentage", "5", "3", "-inf"},
        {"negative-percentage", "5", "3", "-1.0"},
        {"percentage-over-100", "5", "3", "100.01"},
        {"leading-space-percentage", "5", "3", " 50.0"},
        {"trailing-space-percentage", "5", "3", "50.0 "},
        {"tab-percentage", "5", "3", "\t50.0"},
        {"malformed-percentage", "5", "3", "50.0extra"},
        {"empty-percentage", "5", "3", ""},

        {"negative-permissions", "5", "3", "50.0",
         "1730000000", "-1", true},
        {"spaced-permissions", "5", "3", "50.0",
         "1730000000", " 420", true},
        {"out-of-range-permissions", "5", "3", "50.0",
         "1730000000", "1000", true},
        {"overflow-permissions", "5", "3", "50.0",
         "1730000000", overflow, true}
    };

    bool passed = true;
    for (const MetadataCase& test : cases) {
        passed = runCase(root, test) && passed;
    }

    if (!passed) {
        std::cerr << "ARCHIVE METADATA VALIDATION TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL " << cases.size()
              << " ARCHIVE METADATA VALIDATION CASES PASSED\n";
    return 0;
}


#include "application_service.h"
#include "vault_service.h"

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
    }
};

std::string readFile(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        return {};
    }

    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

int main()
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    const fs::path root =
        fs::temp_directory_path() /
        ("sfv_collision_test_" + std::to_string(stamp));

    TempCleanup cleanup{root};

    const fs::path input = root / "input";
    const fs::path archiveRoot = root / "archive";
    const fs::path destination = root / "destination";
    const fs::path source = input / "collision_sample.txt";

    std::error_code error;

    fs::create_directories(input, error);
    if (error) {
        std::cerr << "FAIL: Cannot create input directory.\n";
        return 1;
    }

    fs::create_directories(destination, error);
    if (error) {
        std::cerr << "FAIL: Cannot create destination.\n";
        return 1;
    }

    std::string expectedContent;

    {
        std::ofstream file(source, std::ios::binary);

        if (!file) {
            std::cerr << "FAIL: Cannot create test file.\n";
            return 1;
        }

        for (int i = 0; i < 5000; ++i) {
            const std::string line =
                "SmartFileVault isolated restore test data.\n";

            file << line;
            expectedContent += line;
        }

        if (!file.good()) {
            std::cerr << "FAIL: Cannot write test data.\n";
            return 1;
        }
    }

    fs::last_write_time(
        source,
        fs::file_time_type::clock::now()
            - std::chrono::hours(48),
        error
    );

    if (error) {
        std::cerr << "FAIL: Cannot set test file age.\n";
        return 1;
    }

    ApplicationResult result{};

    if (!runScanAndArchive(
            input.string(),
            1,
            archiveRoot.string(),
            result)) {
        std::cerr << "FAIL: Scan and archive operation failed.\n";
        return 1;
    }

    std::cout
        << "Scanned files: " << result.scannedFiles << '\n'
        << "Archived files: "
        << result.processResult.archivedFiles << '\n';

    if (result.processResult.archivedFiles != 1 ||
        fs::exists(source)) {
        std::cerr
            << "FAIL: Inactive file was not archived "
            << "as expected.\n";
        return 1;
    }

    std::cout << "PASS: Inactive file archived.\n";

    const auto archives = getAllArchives(archiveRoot.string());

    if (archives.size() != 1) {
        std::cerr << "FAIL: Expected exactly one archive.\n";
        return 1;
    }

    const ArchiveMetadata& metadata = archives.front();

    const fs::path existing =
        destination / metadata.originalName;

    const std::string sentinel =
        "KEEP THIS EXISTING FILE UNCHANGED.\n";

    {
        std::ofstream file(existing, std::ios::binary);

        if (!file) {
            std::cerr
                << "FAIL: Cannot create collision test file.\n";
            return 1;
        }

        file << sentinel;

        if (!file.good()) {
            std::cerr
                << "FAIL: Cannot write collision test file.\n";
            return 1;
        }
    }

    std::string restoredPath;

    if (!restoreFromArchive(
            archiveRoot.string(),
            metadata.archiveId,
            destination.string(),
            restoredPath)) {
        std::cerr << "FAIL: Restoration failed.\n";
        return 1;
    }

    const fs::path restored(restoredPath);

    const std::string expectedName =
        fs::path(metadata.originalName).stem().string()
        + " (1)"
        + fs::path(metadata.originalName).extension().string();

    const bool collisionHandled =
        restored.filename() == expectedName &&
        restored != existing &&
        fs::is_regular_file(restored);

    const bool existingPreserved =
        readFile(existing) == sentinel;

    const bool restoredContentMatches =
        readFile(restored) == expectedContent;

    bool passed = true;

    auto check = [&](bool condition, const char* message) {
        std::cout
            << (condition ? "PASS: " : "FAIL: ")
            << message << '\n';

        if (!condition) {
            passed = false;
        }
    };

    check(
        collisionHandled,
        "Collision restored using an alternative filename."
    );

    check(
        existingPreserved,
        "Existing destination file remains unchanged."
    );

    check(
        restoredContentMatches,
        "Restored content matches the archived input."
    );

    std::cout
        << "\nSummary: "
        << (passed ? "ALL TESTS PASSED" : "TEST FAILURE")
        << '\n';

    return passed ? 0 : 1;
}

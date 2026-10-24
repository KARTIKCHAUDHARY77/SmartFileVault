#include "archive.h"
#include "vault_service.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

struct TempCleanup {
    fs::path root;

    ~TempCleanup()
    {
        std::error_code error;
        fs::remove_all(root, error);
    }
};

int main()
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    const fs::path root =
        fs::temp_directory_path() /
        ("sfv_archive_id_validation_" +
         std::to_string(stamp));

    TempCleanup cleanup{root};

    const fs::path archiveRoot = root / "vault";
    const fs::path destination = root / "destination";
    const fs::path source = root / "source.txt";
    const fs::path outsideMetadata = root / "outside.meta";

    std::error_code error;

    fs::create_directories(destination, error);

    if (error) {
        std::cerr << "FAIL: Cannot create test directories.\n";
        return 1;
    }

    {
        std::ofstream file(source, std::ios::binary);

        if (!file) {
            std::cerr << "FAIL: Cannot create source file.\n";
            return 1;
        }

        for (int i = 0; i < 10000; ++i) {
            file << "Synthetic archive ID validation data.\n";
        }

        if (!file.good()) {
            std::cerr << "FAIL: Cannot write source file.\n";
            return 1;
        }
    }

    ArchiveMetadata metadata{};

    if (!createArchive(
            source.string(),
            archiveRoot.string(),
            metadata)) {
        std::cerr << "FAIL: Cannot create synthetic archive.\n";
        return 1;
    }

    // Place valid test metadata outside the vault directory.
    // The archive itself remains inside the isolated test root.
    if (!saveArchiveMetadata(
            outsideMetadata.string(),
            metadata)) {
        std::cerr << "FAIL: Cannot create outside test metadata.\n";
        return 1;
    }

    const std::string maliciousArchiveId = "../../outside";

    std::string restoredPath;

    const bool restored = restoreFromArchive(
        archiveRoot.string(),
        maliciousArchiveId,
        destination.string(),
        restoredPath
    );

    const fs::path expectedOutput =
        destination / metadata.originalName;

    // Vulnerable behavior: the crafted ID locates the outside
    // metadata file and restoration succeeds.
    if (restored || fs::exists(expectedOutput)) {
        std::cerr
            << "REPRODUCED: Traversal archive ID accessed "
            << "metadata outside the vault.\n"
            << "Restore returned: "
            << (restored ? "success" : "failure") << '\n'
            << "Reported output path: "
            << restoredPath << '\n'
            << "Restored output exists: "
            << (fs::exists(expectedOutput) ? "yes" : "no")
            << '\n';

        return 1;
    }

    std::cout
        << "PASS: Traversal archive ID was rejected.\n"
        << "PASS: No output was created in the destination.\n";

    return 0;
}

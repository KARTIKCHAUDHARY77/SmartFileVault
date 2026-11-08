#include "archive.h"
#include "restore.h"

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
        ("sfv_restore_path_validation_" +
         std::to_string(stamp));

    TempCleanup cleanup{root};

    const fs::path archiveRoot = root / "archive";
    const fs::path destination = root / "destination";
    const fs::path source = root / "source.txt";
    const fs::path escaped = root / "escaped.txt";

    std::error_code error;
    fs::create_directories(destination, error);

    if (error) {
        std::cerr << "FAIL: Cannot create test destination.\n";
        return 1;
    }

    {
        std::ofstream file(source, std::ios::binary);

        if (!file) {
            std::cerr << "FAIL: Cannot create test input.\n";
            return 1;
        }

        for (int i = 0; i < 10000; ++i) {
            file << "Synthetic restore path validation data.\n";
        }

        if (!file.good()) {
            std::cerr << "FAIL: Cannot write test input.\n";
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

    fs::remove(source, error);

    if (error) {
        std::cerr << "FAIL: Cannot remove synthetic source file.\n";
        return 1;
    }

    const fs::path metadataPath =
        archiveRoot / "metadata" /
        (metadata.archiveId + ".meta");

    // Simulate an archive metadata record with an unsafe filename.
    metadata.originalName = "../escaped.txt";

    if (!saveArchiveMetadata(metadataPath.string(), metadata)) {
        std::cerr << "FAIL: Cannot write test metadata.\n";
        return 1;
    }

    std::string restoredPath;

    const bool restored = restoreArchive(
        metadataPath.string(),
        destination.string(),
        restoredPath
    );

    // Safe behavior must reject the traversal filename and
    // must not leave output outside the selected destination.
    if (restored || fs::exists(escaped)) {
        std::cerr
            << "REPRODUCED: Unsafe restore path was accepted.\n"
            << "Restore returned: "
            << (restored ? "success" : "failure") << '\n'
            << "Reported output path: " << restoredPath << '\n'
            << "Escaped output exists: "
            << (fs::exists(escaped) ? "yes" : "no") << '\n';

        return 1;
    }

    std::cout
        << "PASS: Traversal filename was rejected.\n"
        << "PASS: No output was created outside the destination.\n";

    return 0;
}

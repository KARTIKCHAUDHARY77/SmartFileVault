#include "archive.h"
#include "restore.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void createTestFile(
    const std::string& path
)
{
    std::ofstream file(path);

    for (int i = 0; i < 5000; i++) {
        file << "SmartFileVault restore test data.\n";
    }
}

int main()
{
    const std::string source =
        "restore_test.txt";

    const std::string archiveFolder =
        "restore_test_archive";

    const std::string restoreFolder =
        "restore_test_destination";

    createTestFile(source);

    ArchiveMetadata metadata;

    if (!createArchive(
            source,
            archiveFolder,
            metadata
        )) {

        std::cout
            << "Archive creation failed.\n";

        fs::remove(source);
        fs::remove_all(archiveFolder);

        return 1;
    }

    fs::create_directories(
        restoreFolder
    );

    std::string metadataPath =
        archiveFolder +
        "/metadata/" +
        metadata.archiveId +
        ".meta";

    std::string restoredPath;

    if (!restoreArchive(
            metadataPath,
            restoreFolder,
            restoredPath
        )) {

        std::cout
            << "Restore failed.\n";

        fs::remove(source);
        fs::remove_all(archiveFolder);
        fs::remove_all(restoreFolder);

        return 1;
    }

    std::cout
        << "Restore completed.\n";

    std::cout
        << "Restored path: "
        << restoredPath
        << '\n';

    std::cout
        << "Restored size: "
        << fs::file_size(restoredPath)
        << " bytes\n";

    if (fs::file_size(restoredPath) ==
        metadata.originalSize) {

        std::cout
            << "Restore size check: PASS\n";
    } else {

        std::cout
            << "Restore size check: FAIL\n";
    }

    fs::remove(source);
    fs::remove_all(archiveFolder);
    fs::remove_all(restoreFolder);

    return 0;
}
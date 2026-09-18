#include "archive.h"
#include "archive_service.h"
#include "file_utils.h"
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

    for (int i = 0; i < 5000; ++i) {
        file << "SmartFileVault safe archive test.\n";
    }
}

int main()
{
    const std::string source =
        "archive_service_test.txt";

    const std::string archiveFolder =
        "archive_service_test_output";

    const std::string restoreFolder =
        "archive_service_restore";

    const std::string reference =
        "archive_service_reference.txt";

    createTestFile(source);

    fs::copy_file(
        source,
        reference,
        fs::copy_options::overwrite_existing
    );

    ArchiveMetadata metadata;

    if (!archiveAndRemoveOriginal(
            source,
            archiveFolder,
            metadata
        )) {

        std::cout
            << "Safe archive failed.\n";

        fs::remove(reference);
        fs::remove_all(archiveFolder);

        return 1;
    }

    std::cout
        << "Safe archive completed.\n";

    if (!fs::exists(source)) {

        std::cout
            << "Original removal: PASS\n";

    } else {

        std::cout
            << "Original removal: FAIL\n";
    }

    if (fs::exists(
            metadata.archivePath
        )) {

        std::cout
            << "Compressed archive: PASS\n";

    } else {

        std::cout
            << "Compressed archive: FAIL\n";

        fs::remove(reference);
        fs::remove_all(archiveFolder);

        return 1;
    }

    std::string metadataPath =
        archiveFolder +
        "/metadata/" +
        metadata.archiveId +
        ".meta";

    if (fs::exists(metadataPath)) {

        std::cout
            << "Metadata file: PASS\n";

    } else {

        std::cout
            << "Metadata file: FAIL\n";
    }

    fs::create_directories(
        restoreFolder
    );

    std::string restoredPath;

    if (!restoreArchive(
            metadataPath,
            restoreFolder,
            restoredPath
        )) {

        std::cout
            << "Restore after archive: FAIL\n";

        fs::remove(reference);
        fs::remove_all(archiveFolder);
        fs::remove_all(restoreFolder);

        return 1;
    }

    std::cout
        << "Restore after archive: PASS\n";

    if (!filesAreDifferent(
            reference,
            restoredPath
        )) {

        std::cout
            << "Restored data check: PASS\n";

    } else {

        std::cout
            << "Restored data check: FAIL\n";
    }

    fs::remove(reference);
    fs::remove_all(archiveFolder);
    fs::remove_all(restoreFolder);

    return 0;
}
#include "archive.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main()
{
    const std::string testFile =
        "archive_test.txt";

    const std::string archiveFolder =
        "archive_test_output";

    {
        std::ofstream file(
            testFile
        );

        for (int i = 0; i < 5000; ++i) {
            file << "SmartFileVault archive test data.\n";
        }
    }

    ArchiveMetadata metadata;

    if (!createArchive(
            testFile,
            archiveFolder,
            metadata
        )) {

        std::cout
            << "Archive creation failed.\n";

        fs::remove(testFile);
        fs::remove_all(archiveFolder);

        return 1;
    }

    std::cout
        << "Archive created successfully.\n";

    std::cout
        << "Archive ID: "
        << metadata.archiveId
        << '\n';

    std::cout
        << "Original file: "
        << metadata.originalName
        << '\n';

    std::cout
        << "Original size: "
        << metadata.originalSize
        << " bytes\n";

    std::cout
        << "Compressed size: "
        << metadata.compressedSize
        << " bytes\n";

    std::cout
        << "Storage saved: "
        << metadata.compressionPercentage
        << "%\n";

    std::cout
        << "Archive path: "
        << metadata.archivePath
        << '\n';

    ArchiveMetadata loaded;

    std::string metadataPath =
        archiveFolder +
        "/metadata/" +
        metadata.archiveId +
        ".meta";

    if (!loadArchiveMetadata(
            metadataPath,
            loaded
        )) {

        std::cout
            << "Metadata load failed.\n";

        fs::remove(testFile);
        fs::remove_all(archiveFolder);

        return 1;
    }

    std::cout
        << "Metadata load: PASS\n";

    std::cout
        << "Loaded Archive ID: "
        << loaded.archiveId
        << '\n';

    fs::remove(testFile);
    fs::remove_all(archiveFolder);

    return 0;
}
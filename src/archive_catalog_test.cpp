#include "archive.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void makeTestFile(
    const std::string& path,
    const std::string& text
)
{
    std::ofstream file(path);

    if (file) {
        for (int i = 0; i < 3000; ++i) {
            file << text << '\n';
        }
    }
}

int main()
{
    const std::string archiveFolder =
        "archive_catalog_test_output";

    const std::string fileOne =
        "catalog_test_one.txt";

    const std::string fileTwo =
        "catalog_test_two.txt";

    makeTestFile(
        fileOne,
        "First SmartFileVault test file"
    );

    makeTestFile(
        fileTwo,
        "Second SmartFileVault test file"
    );

    ArchiveMetadata first;
    ArchiveMetadata second;

    if (!createArchive(
            fileOne,
            archiveFolder,
            first
        )) {

        std::cout
            << "First archive failed.\n";

        fs::remove(fileOne);
        fs::remove(fileTwo);
        fs::remove_all(archiveFolder);

        return 1;
    }

    if (!createArchive(
            fileTwo,
            archiveFolder,
            second
        )) {

        std::cout
            << "Second archive failed.\n";

        fs::remove(fileOne);
        fs::remove(fileTwo);
        fs::remove_all(archiveFolder);

        return 1;
    }


    std::vector<ArchiveMetadata> archives =
        getAllArchives(
            archiveFolder
        );

    std::cout
        << "Archives found: "
        << archives.size()
        << '\n';

    if (archives.size() == 2) {
        std::cout
            << "Archive listing: PASS\n";
    }
    else {
        std::cout
            << "Archive listing: FAIL\n";
    }


    ArchiveMetadata found;

    if (findArchiveById(
            archiveFolder,
            first.archiveId,
            found
        )) {

        std::cout
            << "Find by Archive ID: PASS\n";

        std::cout
            << "Found file: "
            << found.originalName
            << '\n';

    }
    else {

        std::cout
            << "Find by Archive ID: FAIL\n";
    }


    std::uintmax_t originalTotal =
        getTotalOriginalSize(
            archiveFolder
        );

    std::uintmax_t compressedTotal =
        getTotalCompressedSize(
            archiveFolder
        );

    std::uintmax_t savedTotal =
        getTotalSpaceSaved(
            archiveFolder
        );


    std::cout
        << "Total original size: "
        << originalTotal
        << " bytes\n";

    std::cout
        << "Total compressed size: "
        << compressedTotal
        << " bytes\n";

    std::cout
        << "Total space saved: "
        << savedTotal
        << " bytes\n";


    if (originalTotal >
        compressedTotal) {

        std::cout
            << "Storage summary: PASS\n";

    }
    else {

        std::cout
            << "Storage summary: FAIL\n";
    }


    fs::remove(fileOne);
    fs::remove(fileTwo);

    fs::remove_all(
        archiveFolder
    );

    return 0;
}
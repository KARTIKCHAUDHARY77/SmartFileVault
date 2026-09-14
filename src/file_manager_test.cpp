#include "file_manager.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void createTestFile(
    const std::string& filePath
)
{
    std::ofstream file(filePath);

    if (file) {
        file << "SmartFileVault test file.\n";
    }
}

int main()
{
    const std::string testFolder =
        "file_manager_test_data";

    fs::create_directories(
        testFolder
    );

    createTestFile(
        testFolder + "/notes.txt"
    );

    createTestFile(
        testFolder + "/data.csv"
    );

    fs::create_directories(
        testFolder + "/subfolder"
    );

    createTestFile(
        testFolder + "/subfolder/code.cpp"
    );

    FileScanResult result;

    if (!scanDirectory(
            testFolder,
            90,
            result
        )) {

        std::cout
            << "Directory scan failed.\n";

        fs::remove_all(testFolder);

        return 1;
    }

    std::cout
        << "Files found: "
        << result.files.size()
        << '\n';

    std::cout
        << "Files skipped: "
        << result.skippedFiles
        << '\n';

    for (const FileMetadata& file :
         result.files) {

        std::cout
            << "\nName: "
            << file.fileName
            << '\n';

        std::cout
            << "Path: "
            << file.filePath
            << '\n';

        std::cout
            << "Extension: "
            << file.extension
            << '\n';

        std::cout
            << "Size: "
            << file.fileSize
            << " bytes\n";

        std::cout
            << "Inactive: "
            << (file.inactive ? "YES" : "NO")
            << '\n';
    }

    fs::remove_all(testFolder);

    std::cout
        << "\nFile manager test completed.\n";

    return 0;
}
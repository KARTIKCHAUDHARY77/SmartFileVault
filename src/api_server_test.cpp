#include "api_server.h"

#include "archive.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

void createTestFile(const std::string& path)
{
    std::ofstream file(path);

    for (int i = 0; i < 5000; i++) {
        file << "SmartFileVault API test data.\n";
    }

    file.close();
}

int main()
{
    std::string testFolder =
        "api_test_folder";

    std::string archiveFolder =
        "api_test_archive";

    fs::remove_all(testFolder);
    fs::remove_all(archiveFolder);

    fs::create_directories(testFolder);

    std::string testFile =
        testFolder + "/test.txt";

    createTestFile(testFile);

    ArchiveMetadata metadata;

    if (!createArchive(
            testFile,
            archiveFolder,
            metadata)) {

        std::cout << "Could not create test archive.\n";

        fs::remove_all(testFolder);
        fs::remove_all(archiveFolder);

        return 1;
    }

    std::cout << "Test archive created.\n";
    std::cout << "Archive ID: "
              << metadata.archiveId
              << "\n";

    std::cout << "\nStarting API server...\n";
    std::cout << "Use Ctrl + C to stop it.\n\n";

    startApiServer(
        archiveFolder,
        8080
    );

    fs::remove_all(testFolder);
    fs::remove_all(archiveFolder);

    return 0;
}
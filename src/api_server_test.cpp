#include "api_server.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main()
{
    std::string testFolder =
        "api_test_folder";

    std::string archiveFolder =
        "api_test_archive";

    /*
     * Remove only the temporary input folder.
     * Do not remove the archive folder because
     * the API needs to keep archived files available.
     */
    fs::remove_all(testFolder);

    fs::create_directories(testFolder);

    std::ofstream file(
        testFolder + "/test.txt"
    );

    file << "API test file.\n";
    file.close();

    // Make sure the archive root exists.
    fs::create_directories(archiveFolder);

    std::cout
        << "Test folder created.\n";

    std::cout
        << "Starting SmartFileVault API...\n";

    std::cout
        << "Use Ctrl + C to stop the server.\n\n";

    startApiServer(
        archiveFolder,
        8080
    );

    return 0;
}
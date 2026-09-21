#include "inactivity.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main()
{
    std::string testFolder = "inactivity_test_folder";

    fs::remove_all(testFolder);
    fs::create_directories(testFolder);

    std::string filePath = testFolder + "/test.txt";

    std::ofstream file(filePath);
    file << "This is a test file.";
    file.close();

    FileMetadata metadata;

    if (getFileMetadata(filePath, 30, metadata)) {
        std::cout << "File name: " << metadata.fileName << "\n";
        std::cout << "File path: " << metadata.filePath << "\n";
        std::cout << "Extension: " << metadata.extension << "\n";
        std::cout << "File size: " << metadata.size << " bytes\n";

        if (metadata.inactive) {
            std::cout << "Inactive check: INACTIVE\n";
        }
        else {
            std::cout << "Inactive check: ACTIVE\n";
        }
    }
    else {
        std::cout << "Failed to read file metadata.\n";
        fs::remove_all(testFolder);
        return 1;
    }

    fs::remove_all(testFolder);

    return 0;
}
#include "file_manager.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main()
{
    std::string testFolder = "file_manager_test_folder";

    fs::remove_all(testFolder);
    fs::create_directories(testFolder);

    std::ofstream file1(testFolder + "/notes.txt");
    file1 << "Some notes";
    file1.close();

    std::ofstream file2(testFolder + "/project.cpp");
    file2 << "int main() { return 0; }";
    file2.close();

    FileScanResult result;

    if (!scanDirectory(testFolder, 30, result)) {
        std::cout << "Directory scan failed.\n";
        fs::remove_all(testFolder);
        return 1;
    }

    std::cout << "Files found: " << result.files.size() << "\n";
    std::cout << "Files skipped: " << result.skippedFiles << "\n";

    for (const auto& file : result.files) {
        std::cout << "\nFile: " << file.fileName << "\n";
        std::cout << "Path: " << file.filePath << "\n";
        std::cout << "Extension: " << file.extension << "\n";
        std::cout << "Size: " << file.size << " bytes\n";

        if (file.inactive) {
            std::cout << "Status: INACTIVE\n";
        }
        else {
            std::cout << "Status: ACTIVE\n";
        }
    }

    fs::remove_all(testFolder);

    return 0;
}
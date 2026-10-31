#include "scanner.h"

#include <iostream>
#include <ctime>

int main() {
    std::string directoryPath;

    std::cout << "Enter directory path: ";
    std::getline(std::cin, directoryPath);

    std::vector<FileInfo> files = scanDirectory(directoryPath);

    std::cout << "\nFiles found: " << files.size() << "\n\n";

    for (const auto& file : files) {
        std::cout << "Name: " << file.name << "\n";
        std::cout << "Path: " << file.path << "\n";
        std::cout << "Size: " << file.size << " bytes\n";

        std::time_t accessTime = static_cast<std::time_t>(file.lastAccess);

        std::cout << "Last Access: "
                  << std::ctime(&accessTime);

        std::cout << "-------------------------\n";
    }

    return 0;
}
#include <iostream>
#include <vector>

#include "scanner.h"

int main()
{
    std::cout << "=====================================\n";
    std::cout << "          SmartFileVault\n";
    std::cout << " Automated File Compression System\n";
    std::cout << "=====================================\n\n";

    std::string directoryPath;

    std::cout << "Enter directory to scan: ";
    std::getline(std::cin, directoryPath);

    std::vector<FileInfo> files =
        scanDirectory(directoryPath);

    std::cout << "\nFiles found: "
              << files.size()
              << "\n\n";

    for (const auto& file : files)
    {
        std::cout << "Name: " << file.name << "\n";
        std::cout << "Path: " << file.path << "\n";
        std::cout << "Size: " << file.size << " bytes\n";

        std::cout << "-------------------------------------\n";
    }

    return 0;
}
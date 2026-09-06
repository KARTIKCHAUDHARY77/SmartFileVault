#include "scanner.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

std::vector<FileInfo> scanDirectory(const std::string& directoryPath)
{
    std::vector<FileInfo> files;

    if (!fs::exists(directoryPath))
    {
        std::cout << "Directory does not exist: "
                  << directoryPath << std::endl;

        return files;
    }

    for (const auto& entry : fs::directory_iterator(directoryPath))
    {
        if (entry.is_regular_file())
        {
            FileInfo file;

            file.name = entry.path().filename().string();
            file.path = entry.path().string();

            file.size = static_cast<long long>(
                fs::file_size(entry.path())
            );

            files.push_back(file);
        }
    }

    return files;
}
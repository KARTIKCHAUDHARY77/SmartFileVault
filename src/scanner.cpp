#include "scanner.h"

#include <filesystem>
#include <iostream>
#include <sys/stat.h>

namespace fs = std::filesystem;

std::vector<FileInfo> scanDirectory(const std::string& directoryPath) {
    std::vector<FileInfo> files;

    if (!fs::exists(directoryPath)) {
        std::cout << "Directory does not exist.\n";
        return files;
    }

    if (!fs::is_directory(directoryPath)) {
        std::cout << "Path is not a directory.\n";
        return files;
    }

    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        FileInfo file;

        file.name = entry.path().filename().string();
        file.path = entry.path().string();
        file.size = entry.file_size();

        struct stat fileInfo{};

        if (stat(file.path.c_str(), &fileInfo) == 0) {
            file.lastAccess = static_cast<long long>(fileInfo.st_atime);
        } else {
            file.lastAccess = 0;
        }

        files.push_back(file);
    }

    return files;
}
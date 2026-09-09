#ifndef SCANNER_H
#define SCANNER_H

#include <string>
#include <vector>

struct FileInfo {
    std::string name;
    std::string path;
    uintmax_t size;
    long long lastAccess;
};

std::vector<FileInfo> scanDirectory(const std::string& directoryPath);

#endif
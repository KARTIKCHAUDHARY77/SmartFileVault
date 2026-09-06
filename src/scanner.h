#ifndef SCANNER_H
#define SCANNER_H

#include <string>
#include <vector>

struct FileInfo
{
    std::string name;
    std::string path;
    long long size;
};

std::vector<FileInfo> scanDirectory(const std::string& directoryPath);

#endif
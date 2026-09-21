#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "inactivity.h"

#include <string>
#include <vector>

struct FileScanResult {
    std::vector<FileMetadata> files;
    int skippedFiles;
};

bool scanDirectory(const std::string& directoryPath,
                   int inactiveDays,
                   FileScanResult& result);

#endif
#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "inactivity.h"

#include <string>
#include <vector>

struct SkippedFile
{
    std::string filePath;
    std::string reason;
};

struct FileScanResult
{
    std::vector<FileMetadata> files;

    int skippedFiles;

    std::vector<SkippedFile> skippedDetails;
};

bool scanDirectory(const std::string& directoryPath,
                   int inactiveDays,
                   FileScanResult& result);

#endif
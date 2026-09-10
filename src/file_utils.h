#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <cstdint>
#include <string>

bool fileExists(const std::string& filePath);

bool isRegularFile(const std::string& filePath);

std::uintmax_t getFileSize(const std::string& filePath);

bool copyFile(
    const std::string& sourcePath,
    const std::string& destinationPath
);

bool removeFile(const std::string& filePath);

bool filesAreDifferent(
    const std::string& firstPath,
    const std::string& secondPath
);

#endif
#ifndef INACTIVITY_H
#define INACTIVITY_H

#include <cstdint>
#include <string>

struct FileMetadata {
    std::string fileName;
    std::string filePath;
    std::string extension;

    std::uintmax_t size;
    long long lastModified;

    bool inactive;
};

bool isFileInactive(const std::string& filePath, int inactiveDays);

bool getFileMetadata(const std::string& filePath,
                     int inactiveDays,
                     FileMetadata& metadata);

#endif
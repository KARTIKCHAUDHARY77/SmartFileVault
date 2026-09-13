#ifndef INACTIVITY_H
#define INACTIVITY_H

#include <string>
#include <cstdint>
#include <filesystem>

struct FileMetadata {
    std::string fileName;
    std::string filePath;
    std::string extension;

    std::uintmax_t fileSize;

    std::filesystem::file_time_type lastModified;

    bool inactive;
};

bool getFileMetadata(
    const std::string& filePath,
    FileMetadata& metadata
);

bool isFileInactive(
    const FileMetadata& metadata,
    int inactivityDays
);

#endif
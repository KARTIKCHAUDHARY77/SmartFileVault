#include "file_utils.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

bool fileExists(const std::string& filePath) {
    return fs::exists(filePath);
}

bool isRegularFile(const std::string& filePath) {
    return fs::exists(filePath) &&
           fs::is_regular_file(filePath);
}

std::uintmax_t getFileSize(const std::string& filePath) {
    if (!isRegularFile(filePath)) {
        return 0;
    }

    return fs::file_size(filePath);
}

bool copyFile(
    const std::string& sourcePath,
    const std::string& destinationPath
) {
    if (!isRegularFile(sourcePath)) {
        return false;
    }

    std::error_code error;

    fs::copy_file(
        sourcePath,
        destinationPath,
        fs::copy_options::overwrite_existing,
        error
    );

    return !error;
}

bool removeFile(const std::string& filePath) {
    std::error_code error;

    bool removed = fs::remove(filePath, error);

    return removed && !error;
}

bool filesAreDifferent(
    const std::string& firstPath,
    const std::string& secondPath
) {
    if (!isRegularFile(firstPath) ||
        !isRegularFile(secondPath)) {
        return true;
    }

    if (getFileSize(firstPath) != getFileSize(secondPath)) {
        return true;
    }

    std::ifstream first(firstPath, std::ios::binary);
    std::ifstream second(secondPath, std::ios::binary);

    if (!first || !second) {
        return true;
    }

    char firstBuffer[8192];
    char secondBuffer[8192];

    while (first && second) {
        first.read(firstBuffer, sizeof(firstBuffer));
        second.read(secondBuffer, sizeof(secondBuffer));

        std::streamsize firstCount = first.gcount();
        std::streamsize secondCount = second.gcount();

        if (firstCount != secondCount) {
            return true;
        }

        for (std::streamsize i = 0; i < firstCount; ++i) {
            if (firstBuffer[i] != secondBuffer[i]) {
                return true;
            }
        }
    }

    return false;
}
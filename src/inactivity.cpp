#include "inactivity.h"

#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

bool getFileMetadata(
    const std::string& filePath,
    FileMetadata& metadata
) {
    if (!fs::exists(filePath) ||
        !fs::is_regular_file(filePath)) {

        return false;
    }

    std::error_code error;

    fs::path path(filePath);

    metadata.fileName =
        path.filename().string();

    metadata.filePath =
        fs::absolute(path).string();

    metadata.extension =
        path.extension().string();

    metadata.fileSize =
        fs::file_size(path, error);

    if (error) {
        return false;
    }

    metadata.lastModified =
        fs::last_write_time(path, error);

    if (error) {
        return false;
    }

    metadata.inactive = false;

    return true;
}

bool isFileInactive(
    const FileMetadata& metadata,
    int inactivityDays
) {
    if (inactivityDays < 0) {
        return false;
    }

    auto now = fs::file_time_type::clock::now();

    auto age =
        std::chrono::duration_cast<
            std::chrono::hours
        >(
            now - metadata.lastModified
        );

    long long days =
        age.count() / 24;

    return days >= inactivityDays;
}
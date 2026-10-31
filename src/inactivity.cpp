#include "inactivity.h"

#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

bool isFileInactive(const std::string& filePath, int inactiveDays)
{
    try {
        if (!fs::exists(filePath) || !fs::is_regular_file(filePath)) {
            return false;
        }

        auto lastWrite = fs::last_write_time(filePath);
        auto now = fs::file_time_type::clock::now();

        auto fileAge = now - lastWrite;
        auto limit = std::chrono::hours(24 * inactiveDays);

        return fileAge >= limit;
    }
    catch (...) {
        return false;
    }
}

bool getFileMetadata(const std::string& filePath,
                     int inactiveDays,
                     FileMetadata& metadata)
{
    try {
        fs::path path(filePath);

        if (!fs::exists(path) || !fs::is_regular_file(path)) {
            return false;
        }

        metadata.fileName = path.filename().string();
        metadata.filePath = fs::absolute(path).string();
        metadata.extension = path.extension().string();
        metadata.size = fs::file_size(path);

        auto lastWrite = fs::last_write_time(path);

        auto systemTime =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                lastWrite - fs::file_time_type::clock::now()
                + std::chrono::system_clock::now()
            );

        metadata.lastModified =
            std::chrono::system_clock::to_time_t(systemTime);

        metadata.inactive =
            isFileInactive(filePath, inactiveDays);

        return true;
    }
    catch (...) {
        return false;
    }
}
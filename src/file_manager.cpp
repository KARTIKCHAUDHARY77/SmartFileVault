#include "file_manager.h"

#include <filesystem>

namespace fs = std::filesystem;

bool scanDirectory(const std::string& directoryPath,
                   int inactiveDays,
                   FileScanResult& result)
{
    result.files.clear();
    result.skippedFiles = 0;

    try {
        if (!fs::exists(directoryPath) ||
            !fs::is_directory(directoryPath)) {
            return false;
        }

        fs::recursive_directory_iterator iterator(
            directoryPath,
            fs::directory_options::skip_permission_denied
        );

        fs::recursive_directory_iterator end;

        while (iterator != end) {
            try {
                const fs::directory_entry& entry = *iterator;

                if (entry.is_regular_file()) {
                    FileMetadata metadata;

                    if (getFileMetadata(
                            entry.path().string(),
                            inactiveDays,
                            metadata)) {

                        result.files.push_back(metadata);
                    }
                    else {
                        result.skippedFiles++;
                    }
                }
            }
            catch (...) {
                result.skippedFiles++;
            }

            ++iterator;
        }

        return true;
    }
    catch (...) {
        return false;
    }
}
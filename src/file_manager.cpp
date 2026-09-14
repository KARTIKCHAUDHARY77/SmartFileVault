#include "file_manager.h"

#include <filesystem>

namespace fs = std::filesystem;

bool scanDirectory(
    const std::string& directoryPath,
    int inactivityDays,
    FileScanResult& result
)
{
    result.files.clear();
    result.skippedFiles = 0;

    if (!fs::exists(directoryPath) ||
        !fs::is_directory(directoryPath)) {
        return false;
    }

    std::error_code error;

    fs::recursive_directory_iterator iterator(
        directoryPath,
        fs::directory_options::skip_permission_denied,
        error
    );

    fs::recursive_directory_iterator end;

    if (error) {
        return false;
    }

    while (iterator != end) {

        const fs::directory_entry& entry = *iterator;

        std::error_code entryError;

        if (!entry.is_regular_file(entryError)) {

            if (entryError) {
                result.skippedFiles++;
            }

            std::error_code nextError;
            iterator.increment(nextError);

            if (nextError) {
                result.skippedFiles++;
                break;
            }

            continue;
        }

        FileMetadata metadata;

        if (!getFileMetadata(
                entry.path().string(),
                metadata
            )) {

            // File could not be read properly.
            // We skip it and continue with the scan.
            result.skippedFiles++;

        } else {

            metadata.inactive =
                isFileInactive(
                    metadata,
                    inactivityDays
                );

            result.files.push_back(metadata);
        }

        std::error_code nextError;

        iterator.increment(nextError);

        if (nextError) {
            result.skippedFiles++;
            break;
        }
    }

    return true;
}

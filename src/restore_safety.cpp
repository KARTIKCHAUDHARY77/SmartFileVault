#include "restore_safety.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

std::uintmax_t getRestoreSafetySpace(
    const std::string& archiveRoot
)
{
    std::string metadataFolder =
        archiveRoot + "/metadata";

    if (!fs::exists(metadataFolder) ||
        !fs::is_directory(metadataFolder)) {
        return 0;
    }

    std::uintmax_t largestFile = 0;

    std::error_code error;

    for (
        fs::directory_iterator it(
            metadataFolder,
            error
        );
        it != fs::directory_iterator();
        it.increment(error)
    ) {

        if (error) {
            break;
        }

        const fs::directory_entry& entry = *it;

        std::error_code entryError;

        if (!entry.is_regular_file(entryError)) {
            continue;
        }

        if (entry.path().extension() != ".meta") {
            continue;
        }

        ArchiveMetadata metadata;

        if (!loadArchiveMetadata(
                entry.path().string(),
                metadata
            )) {
            continue;
        }

        if (metadata.originalSize > largestFile) {
            largestFile = metadata.originalSize;
        }
    }

    return largestFile;
}

std::uintmax_t getFreeSpace(
    const std::string& path
)
{
    std::error_code error;

    fs::space_info info =
        fs::space(path, error);

    if (error) {
        return 0;
    }

    return info.available;
}

RestoreStorageStatus checkRestoreSpace(
    const ArchiveMetadata& metadata,
    const std::string& destinationPath
)
{
    RestoreStorageStatus status{};

    status.requiredSpace =
        metadata.originalSize;

    status.freeSpace =
        getFreeSpace(destinationPath);

    status.enoughSpace =
        status.freeSpace >=
        status.requiredSpace;

    return status;
}
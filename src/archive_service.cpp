#include "archive_service.h"

#include "compression.h"
#include "file_utils.h"

#include <filesystem>

namespace fs = std::filesystem;

static std::string getMetadataPath(
    const std::string& archiveRoot,
    const std::string& archiveId
)
{
    return archiveRoot +
           "/metadata/" +
           archiveId +
           ".meta";
}

bool archiveAndRemoveOriginal(
    const std::string& filePath,
    const std::string& archiveRoot,
    ArchiveMetadata& metadata
)
{
    if (!fs::exists(filePath) ||
        !fs::is_regular_file(filePath)) {

        return false;
    }

    if (!createArchive(
            filePath,
            archiveRoot,
            metadata
        )) {

        // Original file stays untouched if
        // archive creation fails.
        return false;
    }

    std::string verifyFolder =
        archiveRoot + "/verify";

    std::error_code error;

    fs::create_directories(
        verifyFolder,
        error
    );

    if (error) {
        return false;
    }

    fs::path originalName =
        fs::path(
            metadata.originalName
        );

    std::string verifyPath =
        (
            fs::path(verifyFolder) /
            (
                metadata.archiveId +
                "_" +
                originalName.string()
            )
        ).string();

    if (!decompressFile(
            metadata.archivePath,
            verifyPath
        )) {

        fs::remove(
            verifyPath,
            error
        );

        return false;
    }

    bool filesAreSame =
        !filesAreDifferent(
            filePath,
            verifyPath
        );

    fs::remove(
        verifyPath,
        error
    );

    if (!filesAreSame) {

        // The archive doesn't represent the
        // current original file. Keep original.
        fs::remove(
            metadata.archivePath,
            error
        );

        fs::remove(
            getMetadataPath(
                archiveRoot,
                metadata.archiveId
            ),
            error
        );

        return false;
    }

    /*
     * Only remove the original after the
     * compressed copy has been verified.
     */
    bool removed =
        fs::remove(
            filePath,
            error
        );

    if (!removed || error) {
        return false;
    }

    fs::remove(
        verifyFolder,
        error
    );

    return true;
}
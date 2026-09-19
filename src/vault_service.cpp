#include "vault_service.h"

#include "archive_service.h"
#include "restore.h"
#include "restore_safety.h"

#include <filesystem>

namespace fs = std::filesystem;

bool processCandidate(
    const std::string& filePath,
    const std::string& archiveRoot,
    ArchiveMetadata& metadata
)
{
    if (!fs::exists(filePath) ||
        !fs::is_regular_file(filePath)) {

        return false;
    }

    /*
     * This function assumes that the scanner and
     * compression policy have already decided that
     * this file is a valid compression candidate.
     */
    return archiveAndRemoveOriginal(
        filePath,
        archiveRoot,
        metadata
    );
}


bool restoreFromArchive(
    const std::string& archiveRoot,
    const std::string& archiveId,
    const std::string& destinationDirectory,
    std::string& restoredPath
)
{
    std::string metadataPath =
        archiveRoot +
        "/metadata/" +
        archiveId +
        ".meta";

    if (!fs::exists(metadataPath)) {
        return false;
    }

    ArchiveMetadata metadata;

    if (!loadArchiveMetadata(
            metadataPath,
            metadata
        )) {

        return false;
    }

    /*
     * When destinationDirectory is empty,
     * restoreArchive will try the original path.
     *
     * When the original path is missing,
     * the caller can provide another directory.
     */
    return restoreArchive(
        metadataPath,
        destinationDirectory,
        restoredPath
    );
}


bool getVaultSummary(
    const std::string& archiveRoot,
    VaultSummary& summary
)
{
    summary.archiveCount = 0;
    summary.originalSize = 0;
    summary.compressedSize = 0;
    summary.spaceSaved = 0;
    summary.restoreSafetySpace = 0;
    summary.freeSpace = 0;

    std::vector<ArchiveMetadata> archives =
        getAllArchives(archiveRoot);

    summary.archiveCount =
        archives.size();

    for (const ArchiveMetadata& archive :
         archives) {

        summary.originalSize +=
            archive.originalSize;

        summary.compressedSize +=
            archive.compressedSize;
    }

    if (summary.originalSize >
        summary.compressedSize) {

        summary.spaceSaved =
            summary.originalSize -
            summary.compressedSize;
    }

    summary.restoreSafetySpace =
        getRestoreSafetySpace(
            archiveRoot
        );

    std::string spacePath =
        archiveRoot;

    if (!fs::exists(spacePath)) {
        spacePath = ".";
    }

    summary.freeSpace =
        getFreeSpace(spacePath);

    return true;
}
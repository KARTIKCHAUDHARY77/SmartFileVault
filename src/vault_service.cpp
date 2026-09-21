#include "vault_service.h"

#include "archive_service.h"
#include "compression_policy.h"
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

    CompressionDecision decision =
        getCompressionDecision(filePath);

    if (decision != CompressionDecision::Compress) {
        return false;
    }

    return archiveAndRemoveOriginal(
        filePath,
        archiveRoot,
        metadata
    );
}


bool processInactiveFiles(
    const std::vector<FileMetadata>& files,
    const std::string& archiveRoot,
    ProcessResult& result
)
{
    result.totalFiles =
        static_cast<int>(files.size());

    result.inactiveFiles = 0;
    result.archivedFiles = 0;
    result.skippedFiles = 0;
    result.spaceSaved = 0;

    for (const FileMetadata& file : files) {

        if (!file.inactive) {
            continue;
        }

        result.inactiveFiles++;

        CompressionDecision decision =
            getCompressionDecision(
                file.filePath
            );

        if (decision != CompressionDecision::Compress) {
            result.skippedFiles++;
            continue;
        }

        ArchiveMetadata metadata;

        if (!archiveAndRemoveOriginal(
                file.filePath,
                archiveRoot,
                metadata
            )) {

            result.skippedFiles++;
            continue;
        }

        result.archivedFiles++;

        if (metadata.originalSize >
            metadata.compressedSize) {

            result.spaceSaved +=
                metadata.originalSize -
                metadata.compressedSize;
        }
    }

    return true;
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
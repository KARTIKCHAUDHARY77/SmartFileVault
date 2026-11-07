
#include "vault_service.h"

#include "archive_service.h"
#include "compression_policy.h"
#include "restore.h"
#include "restore_safety.h"

#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>

namespace fs = std::filesystem;

// Serialize vault lifecycle operations within this process so scanning/
// archiving, restore, deletion, and summary reads cannot race one another.
static std::mutex vaultOperationMutex;

bool processCandidate(
    const std::string& filePath,
    const std::string& archiveRoot,
    ArchiveMetadata& metadata
)
{
    std::lock_guard<std::mutex> operationLock(vaultOperationMutex);
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
    std::lock_guard<std::mutex> operationLock(vaultOperationMutex);
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
    std::lock_guard<std::mutex> operationLock(vaultOperationMutex);
    // Reject invalid IDs before constructing any metadata path.
    if (!isSafeArchiveId(archiveId)) {

        restoredPath.clear();

        std::cerr
            << "Restore error: invalid archive ID.\n";

        return false;
    }

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


bool deleteArchiveFromVault(
    const std::string& archiveRoot,
    const std::string& archiveId,
    ArchiveDeletionResult& result
)
{
    std::lock_guard<std::mutex> operationLock(vaultOperationMutex);
    if (!isSafeArchiveId(archiveId)) {
        result = ArchiveDeletionResult{};
        result.errorCode = "invalid_archive_id";
        result.message = "The archive ID is invalid.";
        return false;
    }

    return permanentlyDeleteArchive(archiveRoot, archiveId, result);
}


bool getVaultSummary(
    const std::string& archiveRoot,
    VaultSummary& summary
)
{
    std::lock_guard<std::mutex> operationLock(vaultOperationMutex);
    summary.archiveCount = 0;
    summary.originalSize = 0;
    summary.compressedSize = 0;
    summary.spaceSaved = 0;
    summary.restoreSafetySpace = 0;
    summary.freeSpace = 0;

    const std::vector<ArchiveMetadata> archives =
        getAllArchives(archiveRoot);

    std::uintmax_t originalTotal = 0;
    std::uintmax_t compressedTotal = 0;

    // Do not publish a partial or wrapped summary when archive totals
    // cannot be represented by std::uintmax_t.
    if (!tryGetTotalOriginalSize(archiveRoot, originalTotal) ||
        !tryGetTotalCompressedSize(archiveRoot, compressedTotal)) {
        std::cerr
            << "Vault summary error: archive storage totals overflow.\n";
        summary = VaultSummary{};
        return false;
    }

    summary.archiveCount = archives.size();
    summary.originalSize = originalTotal;
    summary.compressedSize = compressedTotal;

    if (originalTotal > compressedTotal) {
        summary.spaceSaved = originalTotal - compressedTotal;
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

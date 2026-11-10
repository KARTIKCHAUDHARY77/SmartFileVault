
#include "vault_service.h"

#include "archive_service.h"
#include "compression_policy.h"
#include "restore.h"
#include "restore_safety.h"

#include <filesystem>
#include <cerrno>
#include <sys/stat.h>
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


namespace
{
    bool isRealVaultDirectory(const fs::path& path)
    {
        struct stat status {};
        return ::lstat(path.c_str(), &status) == 0 && S_ISDIR(status.st_mode);
    }

    bool isRegularVaultFileNoFollow(const fs::path& path, bool& exists)
    {
        struct stat status {};
        if (::lstat(path.c_str(), &status) == 0) {
            exists = true;
            return S_ISREG(status.st_mode);
        }
        exists = false;
        return errno == ENOENT || errno == ENOTDIR;
    }

    bool safeRestoreMetadataName(const std::string& name)
    {
        if (name.empty() || name == "." || name == ".." ||
            name.find('/') != std::string::npos ||
            name.find('\\') != std::string::npos ||
            name.find('\0') != std::string::npos) {
            return false;
        }
        const fs::path value(name);
        return !value.is_absolute() && !value.has_root_name() &&
               !value.has_root_directory() && !value.has_parent_path() &&
               value.filename() == value;
    }
}

bool restoreFromArchive(
    const std::string& archiveRoot,
    const std::string& archiveId,
    const std::string& destinationDirectory,
    ArchiveRestoreResult& result
)
{
    result = ArchiveRestoreResult{};
    result.archiveId = archiveId;

    std::lock_guard<std::mutex> operationLock(vaultOperationMutex);

    if (!isSafeArchiveId(archiveId)) {
        result.errorCode = "invalid_archive_id";
        result.message = "The archive ID is invalid.";
        return false;
    }

    std::error_code pathError;
    const fs::path root = fs::absolute(archiveRoot, pathError).lexically_normal();
    if (pathError || root.empty()) {
        result.errorCode = "invalid_archive_root";
        result.message = "The vault location could not be resolved.";
        return false;
    }

    const fs::path metadataDirectory = root / "metadata";
    const fs::path compressedDirectory = root / "compressed";
    if (!isRealVaultDirectory(metadataDirectory) ||
        !isRealVaultDirectory(compressedDirectory)) {
        result.errorCode = "vault_directories_unavailable";
        result.message = "The vault metadata or compressed directory is missing or unsafe.";
        return false;
    }

    const fs::path metadataPath = metadataDirectory / (archiveId + ".meta");
    bool metadataExists = false;
    if (!isRegularVaultFileNoFollow(metadataPath, metadataExists)) {
        result.errorCode = "unsafe_metadata_file";
        result.message = "The archive metadata path is not a regular file.";
        return false;
    }
    if (!metadataExists) {
        result.errorCode = "archive_not_found";
        result.message = "No archive metadata exists for this archive ID.";
        return false;
    }

    ArchiveMetadata metadata{};
    if (!loadArchiveMetadata(metadataPath.string(), metadata) ||
        metadata.archiveId != archiveId) {
        result.errorCode = "invalid_metadata";
        result.message = "The archive metadata is malformed or does not match the requested archive ID.";
        return false;
    }

    if (!safeRestoreMetadataName(metadata.originalName)) {
        result.errorCode = "invalid_metadata_name";
        result.message = "The original filename in archive metadata is unsafe.";
        return false;
    }

    const fs::path expectedArchivePath =
        fs::absolute(compressedDirectory /
            (archiveId + "_" + metadata.originalName + ".gz"), pathError)
            .lexically_normal();
    if (pathError || !fs::path(metadata.archivePath).is_absolute() ||
        fs::path(metadata.archivePath).lexically_normal() != expectedArchivePath) {
        result.errorCode = "unsafe_archive_path";
        result.message = "The archive path in metadata does not match its trusted vault location.";
        return false;
    }

    bool compressedExists = false;
    if (!isRegularVaultFileNoFollow(expectedArchivePath, compressedExists)) {
        result.errorCode = "unsafe_compressed_artifact";
        result.message = "The compressed archive path is not a regular file.";
        return false;
    }
    if (!compressedExists) {
        result.errorCode = "archive_not_found";
        result.message = "The compressed archive is missing.";
        return false;
    }

    // restoreArchive performs bounded GZIP validation, exact-size verification,
    // optional SHA-256 verification, permission application and atomic no-overwrite
    // publication. No vault artifact is removed unless it returns successfully.
    if (!restoreArchive(
            metadataPath.string(),
            destinationDirectory,
            result.restoredPath)) {
        result.errorCode = "restore_failed";
        result.message = "The archive could not be restored. The archive and metadata were preserved.";
        return false;
    }

    result.restored = true;

    RestoreArchiveCleanupResult cleanup{};
    const bool cleanupSucceeded = removeRestoredArchiveArtifacts(
        root.string(), archiveId, cleanup
    );
    result.archiveDeleted = cleanup.archiveDeleted;
    result.compressedRemoved = cleanup.compressedRemoved;
    result.metadataRemoved = cleanup.metadataRemoved;
    result.partialFailure = cleanup.partialFailure || !cleanupSucceeded;
    result.errorCode = cleanup.errorCode;
    result.message = cleanupSucceeded
        ? "File restored and archive artifacts removed successfully."
        : "File restored successfully, but archive cleanup was incomplete. " + cleanup.message;

    return true;
}

// Backwards-compatible overload for existing service callers.
bool restoreFromArchive(
    const std::string& archiveRoot,
    const std::string& archiveId,
    const std::string& destinationDirectory,
    std::string& restoredPath
)
{
    restoredPath.clear();
    ArchiveRestoreResult result{};
    const bool restored = restoreFromArchive(
        archiveRoot, archiveId, destinationDirectory, result
    );
    if (restored) {
        restoredPath = result.restoredPath;
    }
    return restored;
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

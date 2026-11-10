#ifndef VAULT_SERVICE_H
#define VAULT_SERVICE_H

#include "archive.h"
#include "archive_service.h"
#include "inactivity.h"

#include <cstdint>
#include <string>
#include <vector>

struct VaultSummary {
    std::size_t archiveCount;

    std::uintmax_t originalSize;
    std::uintmax_t compressedSize;
    std::uintmax_t spaceSaved;

    std::uintmax_t restoreSafetySpace;
    std::uintmax_t freeSpace;
};

struct ArchiveRestoreResult {
    bool restored = false;
    bool archiveDeleted = false;
    bool compressedRemoved = false;
    bool metadataRemoved = false;
    bool partialFailure = false;
    bool reportUpdated = false;
    std::string archiveId;
    std::string restoredPath;
    std::string errorCode;
    std::string message;
};

struct ProcessResult {
    int totalFiles;
    int inactiveFiles;
    int archivedFiles;
    int skippedFiles;

    std::uintmax_t spaceSaved;
};

bool processCandidate(
    const std::string& filePath,
    const std::string& archiveRoot,
    ArchiveMetadata& metadata
);

bool processInactiveFiles(
    const std::vector<FileMetadata>& files,
    const std::string& archiveRoot,
    ProcessResult& result
);

// Structured lifecycle result for the Restore API. A true return means
// the restored file was published; archive cleanup is reported separately.
bool restoreFromArchive(
    const std::string& archiveRoot,
    const std::string& archiveId,
    const std::string& destinationDirectory,
    ArchiveRestoreResult& result
);

bool restoreFromArchive(
    const std::string& archiveRoot,
    const std::string& archiveId,
    const std::string& destinationDirectory,
    std::string& restoredPath
);

bool deleteArchiveFromVault(
    const std::string& archiveRoot,
    const std::string& archiveId,
    ArchiveDeletionResult& result
);

bool getVaultSummary(
    const std::string& archiveRoot,
    VaultSummary& summary
);

#endif
#ifndef ARCHIVE_SERVICE_H
#define ARCHIVE_SERVICE_H

#include "archive.h"

#include <string>

// Restore cleanup removes only the vault's .gz and .meta artifacts. It never
// inspects or deletes metadata.originalPath.
struct RestoreArchiveCleanupResult {
    bool archiveDeleted = false;
    bool compressedRemoved = false;
    bool metadataRemoved = false;
    bool partialFailure = false;
    std::string errorCode;
    std::string message;
};

struct ArchiveDeletionResult {
    bool archiveDeleted = false;
    bool compressedRemoved = false;
    bool metadataRemoved = false;
    bool originalDeleted = false;
    bool originalPreserved = false;
    bool originalMissing = false;
    bool partialFailure = false;
    std::string originalStatus = "not_checked";
    std::string errorCode;
    std::string message;
};

#ifdef SFV_ENABLE_TEST_HOOK
// Test-only callback invoked immediately before archive verification.
void setArchiveServiceTestHook(
    void (*hook)(const std::string& archivePath)
);

// Return true for a named deletion step to simulate an OS-level failure.
// This hook is omitted entirely from normal application builds.
void setArchiveDeletionTestHook(
    bool (*hook)(const std::string& step)
);
#endif

bool archiveAndRemoveOriginal(
    const std::string& filePath,
    const std::string& archiveRoot,
    ArchiveMetadata& metadata
);

// Permanently remove one archive selected by archiveId. The compressed and
// metadata paths are derived from the trusted archive root, not blindly taken
// from metadata. An original is removed only when its recorded identity matches.
// Remove only the compressed artifact and metadata after a verified restore.
bool removeRestoredArchiveArtifacts(
    const std::string& archiveRoot,
    const std::string& archiveId,
    RestoreArchiveCleanupResult& result
);

bool permanentlyDeleteArchive(
    const std::string& archiveRoot,
    const std::string& archiveId,
    ArchiveDeletionResult& result
);

#endif

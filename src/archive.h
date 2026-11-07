#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <cstdint>
#include <string>
#include <vector>

// Filesystem identity captured when the original file is archived.
// Device + inode identify the filesystem object; size and nanosecond
// timestamps make accidental inode reuse or in-place modification less likely
// to be mistaken for the original file. Legacy metadata has no identity.
struct OriginalFileIdentity {
    std::uintmax_t device = 0;
    std::uintmax_t inode = 0;
    std::uintmax_t size = 0;
    long long modifiedSeconds = 0;
    long long modifiedNanoseconds = 0;
    long long changedSeconds = 0;
    long long changedNanoseconds = 0;
    // macOS birth time adds protection against filesystem inode reuse.
    // Other platforms store zero when a birth timestamp is unavailable.
    long long birthSeconds = 0;
    long long birthNanoseconds = 0;
};

struct ArchiveMetadata {
    std::string archiveId;

    std::string originalName;
    std::string originalPath;
    std::string archivePath;

    std::uintmax_t originalSize = 0;
    std::uintmax_t compressedSize = 0;

    double compressionPercentage = 0.0;

    long long archivedAt = 0;

    // Original POSIX rwx bits. Legacy metadata omits this field and restores
    // with the restrictive 0600 fallback.
    std::uint32_t originalPermissions = 0600;
    bool hasOriginalPermissions = false;

    // SHA-256 digest of the original, uncompressed file contents.
    // Legacy metadata files do not contain this field.
    std::string originalSha256;
    bool hasOriginalSha256 = false;

    // Versioned original filesystem identity. Legacy metadata omits this field;
    // for those archives, permanent deletion must preserve any file at the
    // recorded original path because ownership cannot be proven.
    OriginalFileIdentity originalFileIdentity;
    bool hasOriginalFileIdentity = false;
};

// Accept only archive IDs that cannot escape the metadata directory.
bool isSafeArchiveId(const std::string& archiveId);

// Capture and compare identity without following a final-component symlink.
bool captureOriginalFileIdentity(
    const std::string& filePath,
    OriginalFileIdentity& identity
);
bool matchesOriginalFileIdentity(
    const std::string& filePath,
    const OriginalFileIdentity& expected
);

bool createArchive(
    const std::string& inputPath,
    const std::string& archiveRoot,
    ArchiveMetadata& metadata
);

bool saveArchiveMetadata(
    const std::string& metadataPath,
    const ArchiveMetadata& metadata
);

bool loadArchiveMetadata(
    const std::string& metadataPath,
    ArchiveMetadata& metadata
);

/* Archive catalog functions */

std::vector<ArchiveMetadata> getAllArchives(
    const std::string& archiveRoot
);

bool findArchiveById(
    const std::string& archiveRoot,
    const std::string& archiveId,
    ArchiveMetadata& metadata
);

std::uintmax_t getTotalOriginalSize(
    const std::string& archiveRoot
);

std::uintmax_t getTotalCompressedSize(
    const std::string& archiveRoot
);

std::uintmax_t getTotalSpaceSaved(
    const std::string& archiveRoot
);

// Checked totals return false on overflow and set total to zero.
bool tryGetTotalOriginalSize(
    const std::string& archiveRoot,
    std::uintmax_t& total
);

bool tryGetTotalCompressedSize(
    const std::string& archiveRoot,
    std::uintmax_t& total
);

bool tryGetTotalSpaceSaved(
    const std::string& archiveRoot,
    std::uintmax_t& total
);

#endif

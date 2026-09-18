#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <cstdint>
#include <string>
#include <vector>

struct ArchiveMetadata {
    std::string archiveId;

    std::string originalName;
    std::string originalPath;
    std::string archivePath;

    std::uintmax_t originalSize;
    std::uintmax_t compressedSize;

    double compressionPercentage;

    long long archivedAt;
};

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

#endif
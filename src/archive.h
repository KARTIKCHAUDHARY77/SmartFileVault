#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <cstdint>
#include <string>

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

#endif
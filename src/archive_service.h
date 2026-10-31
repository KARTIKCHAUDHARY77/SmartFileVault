#ifndef ARCHIVE_SERVICE_H
#define ARCHIVE_SERVICE_H

#include "archive.h"

#include <string>

bool archiveAndRemoveOriginal(
    const std::string& filePath,
    const std::string& archiveRoot,
    ArchiveMetadata& metadata
);

#endif
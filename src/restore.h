#ifndef RESTORE_H
#define RESTORE_H

#include "archive.h"

#include <string>

bool restoreArchive(
    const std::string& metadataPath,
    const std::string& destinationDirectory,
    std::string& restoredPath
);

#endif
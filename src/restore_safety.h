#ifndef RESTORE_SAFETY_H
#define RESTORE_SAFETY_H

#include "archive.h"

#include <cstdint>
#include <string>

struct RestoreStorageStatus {
    std::uintmax_t requiredSpace;
    std::uintmax_t freeSpace;
    bool enoughSpace;
};

std::uintmax_t getRestoreSafetySpace(
    const std::string& archiveRoot
);

std::uintmax_t getFreeSpace(
    const std::string& path
);

RestoreStorageStatus checkRestoreSpace(
    const ArchiveMetadata& metadata,
    const std::string& destinationPath
);

#endif
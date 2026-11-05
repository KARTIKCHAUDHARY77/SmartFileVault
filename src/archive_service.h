#ifndef ARCHIVE_SERVICE_H
#define ARCHIVE_SERVICE_H

#include "archive.h"

#include <string>

#ifdef SFV_ENABLE_TEST_HOOK
// Test-only callback invoked immediately before archive verification.
void setArchiveServiceTestHook(
    void (*hook)(const std::string& archivePath)
);
#endif

bool archiveAndRemoveOriginal(
    const std::string& filePath,
    const std::string& archiveRoot,
    ArchiveMetadata& metadata
);

#endif
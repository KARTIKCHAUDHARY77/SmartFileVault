#ifndef APPLICATION_SERVICE_H
#define APPLICATION_SERVICE_H

#include "file_manager.h"
#include "vault_service.h"

#include <string>

struct ApplicationResult {
    bool success;

    int scannedFiles;
    int scanSkippedFiles;

    ProcessResult processResult;
};

bool runScanAndArchive(const std::string& directoryPath,
                       int inactiveDays,
                       const std::string& archiveRoot,
                       ApplicationResult& result);

#endif
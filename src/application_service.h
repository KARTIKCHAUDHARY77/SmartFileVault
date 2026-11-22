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

#ifdef SFV_ENABLE_TEST_HOOK
// Test-only hook invoked after scanDirectory() succeeds and before the report
// is published, while runScanAndArchive() still owns the scan report lock.
// Pass nullptr to clear it.
void setScanBeforeReportPublishTestHook(void (*hook)());
#endif

#endif
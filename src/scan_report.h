#ifndef SCAN_REPORT_H
#define SCAN_REPORT_H

#include "file_manager.h"

#include <string>

bool saveScanReport(const FileScanResult& result,
                    const std::string& outputPath);

#endif
#include "application_service.h"

#include <iostream>
#include <string>

int main()
{
    std::cout << "====================================\n";
    std::cout << "       SmartFileVault\n";
    std::cout << "   Smart File Compression System\n";
    std::cout << "====================================\n\n";

    std::string folderPath;

    std::cout << "Enter folder to scan: ";
    std::getline(std::cin, folderPath);

    if (folderPath.empty()) {
        std::cout << "No folder was entered.\n";
        return 1;
    }

    int inactiveDays;

    std::cout << "Enter inactivity period in days: ";
    std::cin >> inactiveDays;
    std::cin.ignore();

    if (inactiveDays <= 0) {
        std::cout << "Inactivity period must be greater than 0.\n";
        return 1;
    }

    std::string archiveRoot;

    std::cout << "Enter archive folder path: ";
    std::getline(std::cin, archiveRoot);

    if (archiveRoot.empty()) {
        std::cout << "No archive folder was entered.\n";
        return 1;
    }

    ApplicationResult result;

    std::cout << "\nStarting SmartFileVault...\n";
    std::cout << "Scanning folder...\n\n";

    if (!runScanAndArchive(
            folderPath,
            inactiveDays,
            archiveRoot,
            result)) {

        std::cout << "SmartFileVault could not complete the operation.\n";
        return 1;
    }

    std::cout << "------------------------------------\n";
    std::cout << "Scan completed successfully.\n";
    std::cout << "------------------------------------\n";

    std::cout << "Files scanned: "
              << result.scannedFiles << "\n";

    std::cout << "Files skipped during scan: "
              << result.scanSkippedFiles << "\n";

    std::cout << "Inactive files: "
              << result.processResult.inactiveFiles << "\n";

    std::cout << "Files archived: "
              << result.processResult.archivedFiles << "\n";

    std::cout << "Files skipped during processing: "
              << result.processResult.skippedFiles << "\n";

    std::cout << "Space saved: "
              << result.processResult.spaceSaved
              << " bytes\n";

    std::cout << "\nSmartFileVault operation completed.\n";

    return 0;
}
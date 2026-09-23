#include "application_service.h"
#include "archive.h"
#include "vault_service.h"

#include <iostream>
#include <string>
#include <vector>

void showArchives(const std::string& archiveRoot)
{
    std::vector<ArchiveMetadata> archives =
        getAllArchives(archiveRoot);

    if (archives.empty()) {
        std::cout << "\nNo archived files found.\n";
        return;
    }

    std::cout << "\nArchived Files\n";
    std::cout << "------------------------------------\n";

    for (const auto& archive : archives) {
        std::cout << "Archive ID : " << archive.archiveId << "\n";
        std::cout << "File       : " << archive.originalName << "\n";
        std::cout << "Original   : " << archive.originalSize << " bytes\n";
        std::cout << "Compressed : " << archive.compressedSize << " bytes\n";
        std::cout << "Saved      : "
                  << archive.compressionPercentage
                  << "%\n";
        std::cout << "Path       : "
                  << archive.originalPath << "\n";
        std::cout << "------------------------------------\n";
    }
}

int main()
{
    std::cout << "====================================\n";
    std::cout << "       SmartFileVault\n";
    std::cout << "   Smart File Compression System\n";
    std::cout << "====================================\n";

    std::string archiveRoot = "SmartFileVault_Vault";

    while (true) {

        std::cout << "\n";
        std::cout << "1. Scan and archive files\n";
        std::cout << "2. Show archived files\n";
        std::cout << "3. Restore a file\n";
        std::cout << "4. Exit\n";

        std::cout << "\nEnter choice: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();

        if (choice == 1) {

            std::string folderPath;
            int inactiveDays;

            std::cout << "\nEnter folder to scan: ";
            std::getline(std::cin, folderPath);

            if (folderPath.empty()) {
                std::cout << "No folder was entered.\n";
                continue;
            }

            std::cout << "Enter inactivity period in days: ";
            std::cin >> inactiveDays;
            std::cin.ignore();

            if (inactiveDays <= 0) {
                std::cout << "Inactivity period must be greater than 0.\n";
                continue;
            }

            ApplicationResult result;

            std::cout << "\nScanning files...\n";

            if (!runScanAndArchive(
                    folderPath,
                    inactiveDays,
                    archiveRoot,
                    result)) {

                std::cout << "The scan and archive operation failed.\n";
                continue;
            }

            std::cout << "\nOperation completed.\n";
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
        }

        else if (choice == 2) {

            showArchives(archiveRoot);
        }

        else if (choice == 3) {

            std::string archiveId;
            std::string destination;

            showArchives(archiveRoot);

            std::cout << "\nEnter Archive ID to restore: ";
            std::getline(std::cin, archiveId);

            if (archiveId.empty()) {
                std::cout << "No Archive ID was entered.\n";
                continue;
            }

            std::cout << "Enter restore destination folder: ";
            std::getline(std::cin, destination);

            if (destination.empty()) {
                std::cout << "No destination was entered.\n";
                continue;
            }

            std::string restoredPath;

            std::cout << "\nRestoring file...\n";

            if (restoreFromArchive(
                    archiveRoot,
                    archiveId,
                    destination,
                    restoredPath)) {

                std::cout << "Restore completed successfully.\n";
                std::cout << "Restored file: "
                          << restoredPath << "\n";
            }
            else {
                std::cout << "Restore failed.\n";
            }
        }

        else if (choice == 4) {

            std::cout << "\nExiting SmartFileVault.\n";
            break;
        }

        else {

            std::cout << "\nInvalid choice.\n";
        }
    }

    return 0;
}
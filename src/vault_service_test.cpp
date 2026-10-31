#include "vault_service.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

void createTestFile(const std::string& path, int lines)
{
    std::ofstream file(path);

    for (int i = 0; i < lines; i++) {
        file << "This is test data for SmartFileVault compression.\n";
    }

    file.close();
}

int main()
{
    std::string testFolder = "vault_test_folder";
    std::string archiveFolder = "vault_test_archive";

    // Remove old test data
    fs::remove_all(testFolder);
    fs::remove_all(archiveFolder);

    fs::create_directories(testFolder);

    // Test file paths
    std::string oldFile = testFolder + "/old_notes.txt";
    std::string recentFile = testFolder + "/recent_notes.txt";
    std::string imageFile = testFolder + "/photo.jpg";

    // Create an inactive test file
    createTestFile(oldFile, 5000);

    // Create a recent active file
    createTestFile(recentFile, 100);

    // Create an already-compressed type
    std::ofstream image(imageFile);
    image << "Fake image data for testing.";
    image.close();

    // Prepare metadata for old file
    FileMetadata oldMetadata;

    oldMetadata.fileName = "old_notes.txt";
    oldMetadata.filePath = fs::absolute(oldFile).string();
    oldMetadata.extension = ".txt";
    oldMetadata.size = fs::file_size(oldFile);
    oldMetadata.lastModified = 0;
    oldMetadata.inactive = true;

    // Prepare metadata for recent file
    FileMetadata recentMetadata;

    recentMetadata.fileName = "recent_notes.txt";
    recentMetadata.filePath = fs::absolute(recentFile).string();
    recentMetadata.extension = ".txt";
    recentMetadata.size = fs::file_size(recentFile);
    recentMetadata.lastModified = 0;
    recentMetadata.inactive = false;

    // Prepare metadata for image file
    FileMetadata imageMetadata;

    imageMetadata.fileName = "photo.jpg";
    imageMetadata.filePath = fs::absolute(imageFile).string();
    imageMetadata.extension = ".jpg";
    imageMetadata.size = fs::file_size(imageFile);
    imageMetadata.lastModified = 0;
    imageMetadata.inactive = true;

    // Put all files into the list
    std::vector<FileMetadata> files;

    files.push_back(oldMetadata);
    files.push_back(recentMetadata);
    files.push_back(imageMetadata);

    ProcessResult result{};

    std::cout << "Starting vault service test...\n\n";

    // Process inactive files
    if (!processInactiveFiles(files, archiveFolder, result)) {
        std::cout << "Vault processing failed.\n";

        fs::remove_all(testFolder);
        fs::remove_all(archiveFolder);

        return 1;
    }

    std::cout << "Total files: "
              << result.totalFiles << "\n";

    std::cout << "Inactive files: "
              << result.inactiveFiles << "\n";

    std::cout << "Archived files: "
              << result.archivedFiles << "\n";

    std::cout << "Skipped files: "
              << result.skippedFiles << "\n";

    std::cout << "Space saved: "
              << result.spaceSaved << " bytes\n\n";

    // Check inactive file
    if (!fs::exists(oldFile)) {
        std::cout << "Inactive file archive test: PASS\n";
    }
    else {
        std::cout << "Inactive file archive test: FAIL\n";
    }

    // Check active file
    if (fs::exists(recentFile)) {
        std::cout << "Active file protection: PASS\n";
    }
    else {
        std::cout << "Active file protection: FAIL\n";
    }

    // Check already-compressed file
    if (fs::exists(imageFile)) {
        std::cout << "Already-compressed file protection: PASS\n";
    }
    else {
        std::cout << "Already-compressed file protection: FAIL\n";
    }

    // Check vault summary
    VaultSummary summary{};

    if (getVaultSummary(archiveFolder, summary)) {

        std::cout << "\nVault summary test: PASS\n";

        std::cout << "Archives: "
                  << summary.archiveCount << "\n";

        std::cout << "Original size: "
                  << summary.originalSize
                  << " bytes\n";

        std::cout << "Compressed size: "
                  << summary.compressedSize
                  << " bytes\n";

        std::cout << "Space saved: "
                  << summary.spaceSaved
                  << " bytes\n";

        std::cout << "Restore safety space: "
                  << summary.restoreSafetySpace
                  << " bytes\n";

        std::cout << "Free space: "
                  << summary.freeSpace
                  << " bytes\n";
    }
    else {
        std::cout << "\nVault summary test: FAIL\n";
    }

    // Remove test folders
    fs::remove_all(testFolder);
    fs::remove_all(archiveFolder);

    std::cout << "\nVault service test completed.\n";

    return 0;
}
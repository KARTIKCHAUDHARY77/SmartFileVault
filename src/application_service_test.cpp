#include "application_service.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

void createTestFile(const std::string& path, int lines)
{
    std::ofstream file(path);

    for (int i = 0; i < lines; i++) {
        file << "This is repeated SmartFileVault test data.\n";
    }

    file.close();
}

int main()
{
    std::string testFolder = "application_test_folder";
    std::string archiveFolder = "application_test_archive";

    fs::remove_all(testFolder);
    fs::remove_all(archiveFolder);

    fs::create_directories(testFolder);

    std::string oldFile =
        testFolder + "/old_notes.txt";

    std::string recentFile =
        testFolder + "/recent_notes.txt";

    std::string imageFile =
        testFolder + "/photo.jpg";

    // Create an old text file
    createTestFile(oldFile, 5000);

    // Create a recent text file
    createTestFile(recentFile, 100);

    // Create an image-type file
    std::ofstream image(imageFile);
    image << "Fake image data.";
    image.close();

    // Make oldFile look 2 days old
    auto oldTime =
        fs::file_time_type::clock::now()
        - std::chrono::hours(48);

    fs::last_write_time(oldFile, oldTime);

    // Make recentFile look very recent
    auto recentTime =
        fs::file_time_type::clock::now();

    fs::last_write_time(recentFile, recentTime);

    ApplicationResult result;

    std::cout << "Starting application workflow test...\n\n";

    bool success = runScanAndArchive(
        testFolder,
        1,
        archiveFolder,
        result
    );

    if (!success) {
        std::cout << "Application workflow failed.\n";

        fs::remove_all(testFolder);
        fs::remove_all(archiveFolder);

        return 1;
    }

    std::cout << "Scan completed.\n";

    std::cout << "Files scanned: "
              << result.scannedFiles << "\n";

    std::cout << "Scan skipped: "
              << result.scanSkippedFiles << "\n";

    std::cout << "Inactive files: "
              << result.processResult.inactiveFiles << "\n";

    std::cout << "Archived files: "
              << result.processResult.archivedFiles << "\n";

    std::cout << "Processing skipped: "
              << result.processResult.skippedFiles << "\n";

    std::cout << "Space saved: "
              << result.processResult.spaceSaved
              << " bytes\n\n";

    // Old inactive file should be archived
    if (!fs::exists(oldFile)) {
        std::cout << "Inactive file processing: PASS\n";
    }
    else {
        std::cout << "Inactive file processing: FAIL\n";
    }

    // Recent file should stay untouched
    if (fs::exists(recentFile)) {
        std::cout << "Active file protection: PASS\n";
    }
    else {
        std::cout << "Active file protection: FAIL\n";
    }

    // JPG should be skipped by compression policy
    if (fs::exists(imageFile)) {
        std::cout << "Already-compressed file protection: PASS\n";
    }
    else {
        std::cout << "Already-compressed file protection: FAIL\n";
    }

    // Check whether the archive folder was created
    if (fs::exists(archiveFolder)) {
        std::cout << "Archive creation: PASS\n";
    }
    else {
        std::cout << "Archive creation: FAIL\n";
    }

    fs::remove_all(testFolder);
    fs::remove_all(archiveFolder);

    std::cout << "\nApplication workflow test completed.\n";

    return 0;
}
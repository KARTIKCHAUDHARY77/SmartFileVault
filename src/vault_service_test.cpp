#include "file_utils.h"
#include "inactivity.h"
#include "vault_service.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

static void createTestFile(
    const std::string& path
)
{
    std::ofstream file(path);

    if (!file) {
        return;
    }

    for (int i = 0; i < 5000; ++i) {
        file << "SmartFileVault integration test data.\n";
    }
}

int main()
{
    const std::string testFolder =
        "vault_integration_test";

    const std::string archiveFolder =
        testFolder + "/archive";

    const std::string oldFile =
        testFolder + "/old_notes.txt";

    const std::string recentFile =
        testFolder + "/recent_notes.txt";

    const std::string imageFile =
        testFolder + "/photo.jpg";


    fs::create_directories(
        testFolder
    );


    createTestFile(oldFile);
    createTestFile(recentFile);
    createTestFile(imageFile);


    FileMetadata oldMetadata;
    FileMetadata recentMetadata;
    FileMetadata imageMetadata;


    if (!getFileMetadata(
            oldFile,
            oldMetadata
        )) {

        std::cout
            << "Old file metadata failed.\n";

        fs::remove_all(testFolder);
        return 1;
    }


    if (!getFileMetadata(
            recentFile,
            recentMetadata
        )) {

        std::cout
            << "Recent file metadata failed.\n";

        fs::remove_all(testFolder);
        return 1;
    }


    if (!getFileMetadata(
            imageFile,
            imageMetadata
        )) {

        std::cout
            << "Image metadata failed.\n";

        fs::remove_all(testFolder);
        return 1;
    }


    /*
     * We are setting the test values manually
     * here so that the integration test can
     * represent an inactive file without
     * waiting for actual days to pass.
     */
    oldMetadata.inactive = true;
    recentMetadata.inactive = false;
    imageMetadata.inactive = true;


    std::vector<FileMetadata> files;

    files.push_back(oldMetadata);
    files.push_back(recentMetadata);
    files.push_back(imageMetadata);


    ProcessResult result;


    if (!processInactiveFiles(
            files,
            archiveFolder,
            result
        )) {

        std::cout
            << "Processing failed.\n";

        fs::remove_all(testFolder);
        return 1;
    }


    std::cout
        << "Total files: "
        << result.totalFiles
        << '\n';

    std::cout
        << "Inactive files: "
        << result.inactiveFiles
        << '\n';

    std::cout
        << "Archived files: "
        << result.archivedFiles
        << '\n';

    std::cout
        << "Skipped files: "
        << result.skippedFiles
        << '\n';

    std::cout
        << "Space saved: "
        << result.spaceSaved
        << " bytes\n";


    if (result.totalFiles == 3) {

        std::cout
            << "File count check: PASS\n";

    } else {

        std::cout
            << "File count check: FAIL\n";
    }


    /*
     * old_notes.txt should be archived because
     * it is inactive and not in the skip list.
     *
     * recent_notes.txt should stay because it
     * is active.
     *
     * photo.jpg should be skipped because it
     * is treated as an already compressed format.
     */

    if (result.inactiveFiles == 2) {

        std::cout
            << "Inactive file check: PASS\n";

    } else {

        std::cout
            << "Inactive file check: FAIL\n";
    }


    if (result.archivedFiles == 1) {

        std::cout
            << "Archive decision check: PASS\n";

    } else {

        std::cout
            << "Archive decision check: FAIL\n";
    }


    if (!fs::exists(oldFile)) {

        std::cout
            << "Original removal check: PASS\n";

    } else {

        std::cout
            << "Original removal check: FAIL\n";
    }


    if (fs::exists(recentFile)) {

        std::cout
            << "Active file protection: PASS\n";

    } else {

        std::cout
            << "Active file protection: FAIL\n";
    }


    if (fs::exists(imageFile)) {

        std::cout
            << "Already-compressed file protection: PASS\n";

    } else {

        std::cout
            << "Already-compressed file protection: FAIL\n";
    }


    fs::remove_all(testFolder);

    return 0;
}
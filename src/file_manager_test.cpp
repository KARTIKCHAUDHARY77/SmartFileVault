#include "file_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

int main()
{
    const std::string testFolder =
        "file_manager_test_folder";

    fs::remove_all(testFolder);
    fs::create_directories(
        testFolder + "/nested"
    );

    const std::string oldFilePath =
        testFolder + "/old_notes.txt";

    const std::string activeFilePath =
        testFolder + "/project.cpp";

    const std::string nestedFilePath =
        testFolder + "/nested/nested.txt";

    const std::string restrictedFilePath =
        testFolder + "/restricted.txt";


    std::ofstream oldFile(oldFilePath);
    oldFile << "Old test notes.";
    oldFile.close();

    std::ofstream activeFile(activeFilePath);
    activeFile << "int main() { return 0; }";
    activeFile.close();

    std::ofstream nestedFile(nestedFilePath);
    nestedFile << "Nested test file.";
    nestedFile.close();

    std::ofstream restrictedFile(restrictedFilePath);
    restrictedFile << "Permission test.";
    restrictedFile.close();


    // Make old_notes.txt older than the 30-day threshold.
    std::error_code timeError;

    fs::last_write_time(
        oldFilePath,
        fs::file_time_type::clock::now()
            - std::chrono::hours(24 * 45),
        timeError
    );

    if (timeError) {
        std::cout
            << "Could not set test file timestamp: "
            << timeError.message()
            << "\n";

        fs::remove_all(testFolder);
        return 1;
    }


    // Try to make one file unreadable.
    // Some environments, including privileged users,
    // may still permit reading it.
    std::error_code permissionError;

    fs::permissions(
        restrictedFilePath,
        fs::perms::none,
        fs::perm_options::replace,
        permissionError
    );

    bool permissionTestEnabled = !permissionError;
    bool restrictedFileReadable = true;

    if (permissionTestEnabled) {
        std::ifstream probe(
            restrictedFilePath,
            std::ios::binary
        );

        restrictedFileReadable = probe.is_open();
        probe.close();
    }


    FileScanResult result;

    if (!scanDirectory(
            testFolder,
            30,
            result)) {
        std::cout << "Directory scan failed.\n";

        fs::permissions(
            restrictedFilePath,
            fs::perms::owner_read
                | fs::perms::owner_write,
            fs::perm_options::replace,
            permissionError
        );

        fs::remove_all(testFolder);
        return 1;
    }


    // Three normal files should always be discovered.
    if (result.files.size() < 3) {
        std::cout
            << "FAIL: Expected at least 3 readable files.\n";

        fs::permissions(
            restrictedFilePath,
            fs::perms::owner_read
                | fs::perms::owner_write,
            fs::perm_options::replace,
            permissionError
        );

        fs::remove_all(testFolder);
        return 1;
    }


    bool foundOldFile = false;
    bool foundNestedFile = false;

    for (const auto& file : result.files) {
        if (file.fileName == "old_notes.txt") {
            foundOldFile = file.inactive;
        }

        if (file.fileName == "nested.txt") {
            foundNestedFile = true;
        }
    }

    if (!foundOldFile) {
        std::cout
            << "FAIL: Old file was not marked inactive.\n";

        fs::permissions(
            restrictedFilePath,
            fs::perms::owner_read
                | fs::perms::owner_write,
            fs::perm_options::replace,
            permissionError
        );

        fs::remove_all(testFolder);
        return 1;
    }

    if (!foundNestedFile) {
        std::cout
            << "FAIL: Nested file was not discovered.\n";

        fs::permissions(
            restrictedFilePath,
            fs::perms::owner_read
                | fs::perms::owner_write,
            fs::perm_options::replace,
            permissionError
        );

        fs::remove_all(testFolder);
        return 1;
    }


    if (permissionTestEnabled
        && !restrictedFileReadable) {

        bool recordedRestrictedFile =
            std::any_of(
                result.skippedDetails.begin(),
                result.skippedDetails.end(),
                [&](const SkippedFile& item) {
                    return item.filePath ==
                           restrictedFilePath;
                }
            );

        if (!recordedRestrictedFile) {
            std::cout
                << "FAIL: Unreadable file was not recorded.\n";

            fs::permissions(
                restrictedFilePath,
                fs::perms::owner_read
                    | fs::perms::owner_write,
                fs::perm_options::replace,
                permissionError
            );

            fs::remove_all(testFolder);
            return 1;
        }
    }


    std::cout << "PASS: Recursive scanning.\n";
    std::cout << "PASS: Inactivity classification.\n";
    std::cout << "Files discovered: "
              << result.files.size() << "\n";

    std::cout << "Skipped items: "
              << result.skippedFiles << "\n";


    // Invalid input should fail without leaving stale results.
    FileScanResult invalidResult = result;

    bool invalidScanSucceeded =
        scanDirectory(
            testFolder + "/does_not_exist",
            30,
            invalidResult
        );

    if (invalidScanSucceeded
        || !invalidResult.files.empty()
        || invalidResult.skippedFiles != 0
        || !invalidResult.skippedDetails.empty()) {

        std::cout
            << "FAIL: Invalid scan was not handled correctly.\n";

        fs::permissions(
            restrictedFilePath,
            fs::perms::owner_read
                | fs::perms::owner_write,
            fs::perm_options::replace,
            permissionError
        );

        fs::remove_all(testFolder);
        return 1;
    }

    std::cout << "PASS: Invalid directory handling.\n";


    fs::permissions(
        restrictedFilePath,
        fs::perms::owner_read
            | fs::perms::owner_write,
        fs::perm_options::replace,
        permissionError
    );

    fs::remove_all(testFolder);

    std::cout
        << "\nAll file manager tests passed.\n";

    return 0;
}
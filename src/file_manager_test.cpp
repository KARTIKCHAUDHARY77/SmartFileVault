
#include "file_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

namespace fs = std::filesystem;

int main()
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    const fs::path testFolder =
        fs::temp_directory_path() /
        ("sfv_file_manager_test_" + std::to_string(stamp));

    bool passed = true;

    const auto check = [&](bool condition, const std::string& message) {
        std::cout << (condition ? "PASS: " : "FAIL: ")
                  << message << '\n';

        if (!condition) {
            passed = false;
        }
    };

    try {
        if (!fs::create_directories(testFolder)) {
            std::cerr << "Could not create unique test directory.\n";
            return 1;
        }

        {
            std::ofstream file(testFolder / "notes.txt");
            file << "Some notes";

            if (!file) {
                std::cerr << "Could not create notes.txt.\n";
                std::error_code cleanupError;
                fs::remove_all(testFolder, cleanupError);
                return 1;
            }
        }

        {
            std::ofstream file(testFolder / "project.cpp");
            file << "int main() { return 0; }";

            if (!file) {
                std::cerr << "Could not create project.cpp.\n";
                std::error_code cleanupError;
                fs::remove_all(testFolder, cleanupError);
                return 1;
            }
        }

        FileScanResult result{};

        check(
            scanDirectory(testFolder.string(), 30, result),
            "valid directory scan succeeds"
        );

        check(
            result.files.size() == 2,
            "both regular files are discovered"
        );

        check(
            result.skippedFiles == 0,
            "no files are skipped in a normal scan"
        );

        check(
            result.skippedDetails.empty(),
            "no skipped details for a normal scan"
        );

        check(
            result.skippedFiles ==
                static_cast<int>(result.skippedDetails.size()),
            "skipped count matches skipped details"
        );

        std::set<std::string> names;
        std::set<std::string> extensions;
        bool sizesCorrect = true;

        for (const auto& file : result.files) {
            names.insert(file.fileName);
            extensions.insert(file.extension);

            if (file.fileName == "notes.txt") {
                sizesCorrect = sizesCorrect && file.size == 10;
            }
            else if (file.fileName == "project.cpp") {
                sizesCorrect = sizesCorrect && file.size == 24;
            }
        }

        check(
            names.count("notes.txt") == 1 &&
                names.count("project.cpp") == 1,
            "expected filenames are returned"
        );

        check(
            extensions.count(".txt") == 1 &&
                extensions.count(".cpp") == 1,
            "expected extensions are returned"
        );

        check(
            sizesCorrect,
            "file sizes match written contents"
        );

        FileScanResult invalidResult{};

        check(
            !scanDirectory(
                (testFolder / "missing-directory").string(),
                30,
                invalidResult
            ),
            "missing directory is rejected"
        );

        check(
            !scanDirectory(testFolder.string(), 0, invalidResult),
            "zero inactivity days are rejected"
        );

        check(
            !scanDirectory("", 30, invalidResult),
            "empty directory path is rejected"
        );
    }
    catch (const std::exception& error) {
        std::cerr << "Test setup/runtime error: "
                  << error.what() << '\n';
        passed = false;
    }

    std::error_code removeError;
    fs::remove_all(testFolder, removeError);

    if (removeError) {
        std::cerr << "Warning: could not clean test directory: "
                  << removeError.message() << '\n';
        passed = false;
    }

    if (passed) {
        std::cout << "All file manager tests passed.\n";
        return 0;
    }

    std::cerr << "File manager tests failed.\n";
    return 1;
}

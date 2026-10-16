
#include "scan_report.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

int main()
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    const fs::path testRoot =
        fs::temp_directory_path() /
        ("smartfilevault_report_test_" + std::to_string(stamp));

    std::error_code error;

    if (!fs::create_directories(testRoot, error) || error) {
        std::cout << "FAIL: Could not create temporary test folder.\n";
        return 1;
    }

    const fs::path reportPath = testRoot / "report.json";

    auto cleanup = [&]() {
        std::error_code cleanupError;
        fs::remove_all(testRoot, cleanupError);
    };

    FileScanResult result{};

    FileMetadata oldFile{};
    oldFile.fileName = "report\"test\n.txt";
    oldFile.filePath = "/tmp/folder\\notes\t.txt";
    oldFile.extension = ".txt";
    oldFile.extension.push_back(static_cast<char>(1));
    oldFile.extension += "x";
    oldFile.size = 1536;
    oldFile.lastModified = 123456789;
    oldFile.inactive = true;

    FileMetadata activeFile{};
    activeFile.fileName = "project.cpp";
    activeFile.filePath = "/tmp/project.cpp";
    activeFile.extension = ".cpp";
    activeFile.size = 25;
    activeFile.lastModified = 123456799;
    activeFile.inactive = false;

    result.files.push_back(oldFile);
    result.files.push_back(activeFile);

    result.skippedDetails.push_back({
        "/tmp/unreadable\\notes.txt",
        "Permission denied: \"read\""
    });
    result.skippedFiles =
        static_cast<int>(result.skippedDetails.size());

    if (!saveScanReport(result, reportPath.string())) {
        std::cout << "FAIL: Could not create scan report.\n";
        cleanup();
        return 1;
    }

    std::ifstream report(reportPath, std::ios::binary);

    if (!report.is_open()) {
        std::cout << "FAIL: Could not reopen scan report.\n";
        cleanup();
        return 1;
    }

    const std::string contents(
        (std::istreambuf_iterator<char>(report)),
        std::istreambuf_iterator<char>()
    );

    report.close();

    bool passed = true;

    auto check = [&](bool condition, const std::string& description) {
        if (condition) {
            std::cout << "PASS: " << description << "\n";
        }
        else {
            std::cout << "FAIL: " << description << "\n";
            passed = false;
        }
    };

    auto contains = [&](const std::string& value) {
        return contents.find(value) != std::string::npos;
    };

    check(contains("\"totalFiles\": 2"), "Total file count");
    check(contains("\"inactiveFiles\": 1"), "Inactive file count");
    check(contains("\"activeFiles\": 1"), "Active file count");
    check(contains("\"skippedFiles\": 1"), "Skipped file count");

    check(
        contains("\"skippedDetails\": ["),
        "Skipped details array"
    );

    check(
        contains("\"filePath\": \"/tmp/unreadable\\\\notes.txt\"") &&
        contains("\"reason\": \"Permission denied: \\\"read\\\"\""),
        "Skipped path and reason are JSON-escaped"
    );

    check(
        contains("\"status\": \"inactive\""),
        "Inactive status"
    );

    check(
        contains("\"status\": \"active\""),
        "Active status"
    );

    check(
        contains("\"sizeText\": \"1.50 KB\""),
        "Readable file size"
    );

    check(
        contains("\"fileName\": \"report\\\"test\\n.txt\""),
        "Quote and newline escaping"
    );

    check(
        contains("\"filePath\": \"/tmp/folder\\\\notes\\t.txt\""),
        "Backslash and tab escaping"
    );

    check(
        contains("\"extension\": \".txt\\u0001x\""),
        "Control character escaping"
    );

    // A report must not be created under a missing parent directory.
    check(
        !saveScanReport(
            result,
            (testRoot / "missing" / "report.json").string()
        ),
        "Invalid output path handling"
    );

    cleanup();

    if (!passed) {
        std::cout << "\nSome scan report tests failed.\n";
        return 1;
    }

    std::cout << "\nAll scan report tests passed.\n";
    return 0;
}

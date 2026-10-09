#include "scan_report.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

int main()
{
    const std::string reportPath =
        "scan_report_test.json";

    const std::string missingDirectory =
        "scan_report_missing_directory";

    fs::remove(reportPath);
    fs::remove_all(missingDirectory);

    FileScanResult result{};

    FileMetadata oldFile{};

    oldFile.fileName = "report\"test\n.txt";
    oldFile.filePath = "/tmp/folder\\notes\t.txt";

    // Include a control character to test JSON escaping.
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

    result.skippedFiles = 1;

    SkippedFile skipped{};

    skipped.filePath = "/tmp/bad\tname.txt";
    skipped.reason = "Permission denied\nread failed\\again";

    result.skippedDetails.push_back(skipped);


    if (!saveScanReport(result, reportPath))
    {
        std::cout << "FAIL: Could not create scan report.\n";
        return 1;
    }

    std::ifstream report(reportPath);

    if (!report.is_open())
    {
        std::cout << "FAIL: Could not read scan report.\n";
        fs::remove(reportPath);
        return 1;
    }

    std::string contents(
        (std::istreambuf_iterator<char>(report)),
        std::istreambuf_iterator<char>()
    );

    report.close();

    bool passed = true;

    auto check = [&](bool condition,
                     const std::string& description)
    {
        if (condition)
        {
            std::cout << "PASS: " << description << "\n";
        }
        else
        {
            std::cout << "FAIL: " << description << "\n";
            passed = false;
        }
    };

    auto contains = [&](const std::string& text)
    {
        return contents.find(text) != std::string::npos;
    };


    check(
        contains("\"totalFiles\": 2"),
        "Total file count"
    );

    check(
        contains("\"inactiveFiles\": 1"),
        "Inactive file count"
    );

    check(
        contains("\"activeFiles\": 1"),
        "Active file count"
    );

    check(
        contains("\"skippedFiles\": 1"),
        "Skipped file count"
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
        "Other control character escaping"
    );

    check(
        contains("\"reason\": \"Permission denied\\nread failed\\\\again\""),
        "Skipped-file reason escaping"
    );

    check(
        contains("\"skippedDetails\": ["),
        "Skipped details section"
    );


    // Writing into a directory that does not exist must fail.
    check(
        !saveScanReport(
            result,
            missingDirectory + "/report.json"
        ),
        "Invalid output path handling"
    );


    fs::remove(reportPath);
    fs::remove_all(missingDirectory);

    if (!passed)
    {
        std::cout << "\nSome scan report tests failed.\n";
        return 1;
    }

    std::cout << "\nAll scan report tests passed.\n";
    return 0;
}
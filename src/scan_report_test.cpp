#include "scan_report.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main()
{
    std::string testFolder = "scan_report_test_folder";
    std::string reportPath = "scan_report_test.json";

    fs::remove_all(testFolder);
    fs::remove(reportPath);

    fs::create_directories(testFolder);

    std::ofstream file1(testFolder + "/notes.txt");
    file1 << "Some test notes.";
    file1.close();

    std::ofstream file2(testFolder + "/project.cpp");
    file2 << "int main() { return 0; }";
    file2.close();

    FileScanResult result;

    if (!scanDirectory(testFolder, 30, result)) {
        std::cout << "Scan failed.\n";

        fs::remove_all(testFolder);
        return 1;
    }

    if (!saveScanReport(result, reportPath)) {
        std::cout << "Report creation failed.\n";

        fs::remove_all(testFolder);
        return 1;
    }

    std::cout << "Scan report created successfully.\n";
    std::cout << "Files found: "
              << result.files.size() << "\n";
    std::cout << "Files skipped: "
              << result.skippedFiles << "\n";

    std::ifstream report(reportPath);

    if (report.is_open()) {
        std::cout << "\nGenerated JSON:\n";
        std::cout << "------------------------------\n";

        std::string line;

        while (std::getline(report, line)) {
            std::cout << line << "\n";
        }

        report.close();
    }
    else {
        std::cout << "Could not open generated report.\n";
    }

    fs::remove_all(testFolder);
    fs::remove(reportPath);

    std::cout << "\nScan report test completed.\n";

    return 0;
}
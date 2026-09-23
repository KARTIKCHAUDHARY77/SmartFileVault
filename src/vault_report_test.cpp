#include "vault_report.h"

#include "archive.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

void createTestFile(const std::string& path)
{
    std::ofstream file(path);

    for (int i = 0; i < 5000; i++) {
        file << "SmartFileVault vault report test data.\n";
    }

    file.close();
}

int main()
{
    std::string testFolder = "vault_report_test_folder";
    std::string archiveFolder = "vault_report_test_archive";
    std::string reportPath = "vault_report_test.json";

    fs::remove_all(testFolder);
    fs::remove_all(archiveFolder);
    fs::remove(reportPath);

    fs::create_directories(testFolder);

    std::string testFile =
        testFolder + "/old_notes.txt";

    createTestFile(testFile);

    ArchiveMetadata metadata;

    std::cout << "Creating test archive...\n";

    if (!createArchive(
            testFile,
            archiveFolder,
            metadata)) {

        std::cout << "Archive creation failed.\n";

        fs::remove_all(testFolder);
        fs::remove_all(archiveFolder);

        return 1;
    }

    std::cout << "Archive created.\n";
    std::cout << "Archive ID: "
              << metadata.archiveId << "\n";

    if (!saveVaultReport(
            archiveFolder,
            reportPath)) {

        std::cout << "Vault report creation failed.\n";

        fs::remove_all(testFolder);
        fs::remove_all(archiveFolder);

        return 1;
    }

    std::cout << "\nVault report created successfully.\n";

    std::ifstream report(reportPath);

    if (report.is_open()) {

        std::string line;

        std::cout << "\nGenerated JSON:\n";
        std::cout << "------------------------------------\n";

        while (std::getline(report, line)) {
            std::cout << line << "\n";
        }

        report.close();
    }
    else {
        std::cout << "Could not open report.\n";
    }

    fs::remove_all(testFolder);
    fs::remove_all(archiveFolder);
    fs::remove(reportPath);

    std::cout << "\nVault report test completed.\n";

    return 0;
}
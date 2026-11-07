#include "archive.h"
#include "vault_report.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: vault_report_json_test TEMP_DIRECTORY\n";
        return 2;
    }

    const fs::path root(argv[1]);
    std::error_code error;
    fs::create_directories(root, error);
    if (error) {
        std::cerr << "FAIL: could not create fixture directory: "
                  << error.message() << '\n';
        return 1;
    }

    // Tabs and quotes are legal POSIX filename characters. The generated
    // vault report must escape them so the output remains valid JSON.
    const fs::path source = root / "special\t\"name.txt";
    {
        std::ofstream file(source, std::ios::binary | std::ios::trunc);
        if (!file) {
            std::cerr << "FAIL: could not create source fixture\n";
            return 1;
        }
        for (int i = 0; i < 3000; ++i) {
            file << "SmartFileVault JSON escaping regression fixture.\n";
        }
        if (!file.good()) {
            std::cerr << "FAIL: could not write source fixture\n";
            return 1;
        }
    }

    const fs::path vault = root / "vault";
    ArchiveMetadata metadata{};

    if (!createArchive(source.string(), vault.string(), metadata)) {
        std::cerr << "FAIL: could not create archive fixture\n";
        return 1;
    }

    const fs::path report = root / "vault_report.json";
    if (!saveVaultReport(vault.string(), report.string())) {
        std::cerr << "FAIL: could not generate vault report\n";
        return 1;
    }

    if (!fs::is_regular_file(report, error) || error) {
        std::cerr << "FAIL: report file was not published\n";
        return 1;
    }

    std::cout << "PASS: archive with tab and quote filename created\n";
    std::cout << "PASS: vault report generated\n";
    std::cout << report.string() << '\n';
    return 0;
}

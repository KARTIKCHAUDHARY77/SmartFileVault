#include "file_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

namespace fs = std::filesystem;

static bool writeFile(const fs::path& path, const std::string& contents)
{
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << contents;
    return static_cast<bool>(file);
}

int main()
{
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    const fs::path root =
        fs::temp_directory_path() /
        ("sfv_vault_exclusion_test_" + std::to_string(stamp));

    const fs::path vault = root / "vault";
    const fs::path sibling = root / "vault-backup";

    bool passed = true;

    const auto check = [&](bool condition, const std::string& message) {
        std::cout << (condition ? "PASS: " : "FAIL: ")
                  << message << '\n';
        if (!condition) {
            passed = false;
        }
    };

    try {
        fs::create_directories(root);

        check(writeFile(root / "keep.txt", "normal source"),
              "normal root file created");

        check(writeFile(root / "nested" / "keep2.txt", "nested source"),
              "normal nested file created");

        check(writeFile(sibling / "sibling.txt", "sibling source"),
              "similarly named sibling file created");

        check(writeFile(vault / "compressed" / "old.gz", "archive bytes"),
              "vault compressed file created");

        check(writeFile(vault / "metadata" / "old.meta", "metadata bytes"),
              "vault metadata file created");

        check(writeFile(vault / "nested" / "vault_report.json", "{}"),
              "vault report created");

        FileScanResult result{};

        check(
            scanDirectory(root.string(), 30, result, vault.string()),
            "scan succeeds with vault exclusion"
        );

        std::set<std::string> discovered;
        for (const auto& file : result.files) {
            discovered.insert(file.fileName);
        }

        check(
            discovered.count("keep.txt") == 1 &&
            discovered.count("keep2.txt") == 1 &&
            discovered.count("sibling.txt") == 1,
            "normal files and similarly named sibling are discovered"
        );

        check(
            discovered.count("old.gz") == 0 &&
            discovered.count("old.meta") == 0 &&
            discovered.count("vault_report.json") == 0,
            "vault contents are excluded"
        );

        check(
            result.skippedFiles == 0 && result.skippedDetails.empty(),
            "excluded vault files do not count as skipped"
        );

        check(
            fs::exists(vault / "compressed" / "old.gz") &&
            fs::exists(vault / "metadata" / "old.meta") &&
            fs::exists(vault / "nested" / "vault_report.json"),
            "excluded vault files remain untouched"
        );

        FileScanResult rejected{};

        check(
            !scanDirectory(vault.string(), 30, rejected, vault.string()),
            "scan rooted at the vault is rejected"
        );

        check(
            !scanDirectory(
                (vault / "nested").string(), 30, rejected, vault.string()
            ),
            "scan rooted inside the vault is rejected"
        );
    }
    catch (const std::exception& error) {
        std::cerr << "Test setup/runtime error: "
                  << error.what() << '\n';
        passed = false;
    }

    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);

    if (cleanupError) {
        std::cerr << "Cleanup warning: "
                  << cleanupError.message() << '\n';
        passed = false;
    }

    if (passed) {
        std::cout << "All vault exclusion tests passed.\n";
        return 0;
    }

    std::cerr << "Vault exclusion tests failed.\n";
    return 1;
}

#include "application_service.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <set>

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
        ("sfv_application_vault_test_" + std::to_string(stamp));

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

        check(writeFile(root / "source.txt", "recent source file"),
              "source file created");

        check(writeFile(sibling / "backup.txt", "ordinary sibling file"),
              "similarly named sibling file created");

        ApplicationResult first{};

        check(
            runScanAndArchive(root.string(), 30, vault.string(), first),
            "first application workflow succeeds and creates vault"
        );

        check(
            first.scannedFiles == 2,
            "first scan discovers both ordinary files"
        );

        check(
            fs::exists(vault / "scan_report.json"),
            "first workflow creates vault scan report"
        );

        check(
            writeFile(vault / "compressed" / "old.gz", "archive bytes"),
            "vault archive fixture created"
        );

        check(
            writeFile(vault / "metadata" / "old.meta", "metadata bytes"),
            "vault metadata fixture created"
        );

        check(
            writeFile(vault / "nested" / "vault_report.json", "{}"),
            "nested vault report fixture created"
        );

        ApplicationResult second{};

        check(
            runScanAndArchive(root.string(), 30, vault.string(), second),
            "second application workflow succeeds"
        );

        check(
            second.scannedFiles == 2,
            "second scan discovers only the two ordinary files"
        );

        check(
            second.scanSkippedFiles == 0,
            "excluded vault contents do not count as skipped"
        );

        check(
            fs::exists(root / "source.txt") &&
            fs::exists(sibling / "backup.txt"),
            "ordinary source files remain untouched"
        );

        check(
            fs::exists(vault / "compressed" / "old.gz") &&
            fs::exists(vault / "metadata" / "old.meta") &&
            fs::exists(vault / "nested" / "vault_report.json"),
            "existing vault contents remain untouched"
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
        std::cout << "All application vault exclusion tests passed.\n";
        return 0;
    }

    std::cerr << "Application vault exclusion tests failed.\n";
    return 1;
}

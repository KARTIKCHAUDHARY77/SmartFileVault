#include "file_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

static bool writeFile(const fs::path& path, const std::string& value)
{
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file << value;
    file.close();
    return !file.fail();
}

static bool check(bool condition, const std::string& message)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    return condition;
}

int main()
{
    const auto stamp = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("sfv_temp_output_scan_" + std::to_string(stamp));
    std::error_code error;
    fs::create_directories(root, error);
    if (error) {
        std::cerr << "FAIL: cannot create test root: " << error.message() << '\n';
        return 1;
    }

    const fs::path active = root / "active.txt";
    const fs::path stale = root / "stale.txt";
    const fs::path rootTemporary = root / ".sfv-output-root.tmp";
    const fs::path nestedTemporary = root / "nested" / ".sfv-output-nested.tmp";
    const fs::path stagingDirectory = root / "nested" / ".sfv-output-staging";
    const fs::path stagingPayload = stagingDirectory / "incomplete.bin";

    bool setup = writeFile(active, "active data") &&
                 writeFile(stale, "stale data") &&
                 writeFile(rootTemporary, "partial root output") &&
                 writeFile(nestedTemporary, "partial nested output") &&
                 writeFile(stagingPayload, "temporary staging data");

    if (!setup) {
        std::cerr << "FAIL: could not create all test fixtures\n";
        fs::remove_all(root, error);
        return 1;
    }

    fs::last_write_time(
        stale,
        fs::file_time_type::clock::now() - std::chrono::hours(24 * 60),
        error
    );
    if (error) {
        std::cerr << "FAIL: could not age stale fixture: " << error.message() << '\n';
        fs::remove_all(root, error);
        return 1;
    }

    FileScanResult result{};
    bool passed = true;
    passed = check(scanDirectory(root.string(), 30, result),
                   "scan completes with temporary outputs present") && passed;

    std::set<std::string> names;
    for (const auto& item : result.files) names.insert(item.fileName);

    passed = check(names.count("active.txt") == 1,
                   "active file remains visible to scanner") && passed;
    passed = check(names.count("stale.txt") == 1,
                   "stale file remains visible to scanner") && passed;
    passed = check(names.count(".sfv-output-root.tmp") == 0 &&
                   names.count(".sfv-output-nested.tmp") == 0,
                   "root and nested temporary output files are ignored") && passed;
    passed = check(names.count("incomplete.bin") == 0,
                   "contents of a temporary staging directory are not scanned") && passed;
    passed = check(fs::exists(rootTemporary) && fs::exists(nestedTemporary) &&
                   fs::exists(stagingDirectory) && fs::exists(stagingPayload),
                   "scanner leaves temporary files and directories untouched") && passed;
    passed = check(result.skippedFiles == 0 && result.skippedDetails.empty(),
                   "intentional temporary-output exclusions do not pollute skipped-file errors") && passed;

    fs::remove_all(root, error);
    passed = check(!error, "test fixtures are cleaned up") && passed;

    if (!passed) {
        std::cerr << "TEMPORARY OUTPUT SCANNER TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL TEMPORARY OUTPUT SCANNER TESTS PASSED\n";
    return 0;
}

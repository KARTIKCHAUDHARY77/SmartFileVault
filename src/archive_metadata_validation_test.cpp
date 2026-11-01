#include "archive.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

static fs::path createTestDirectory()
{
    char pattern[] = "/tmp/sfv-metadata-validation.XXXXXX";
    char* result = ::mkdtemp(pattern);
    return result == nullptr ? fs::path{} : fs::path(result);
}

static bool writeMetadata(
    const fs::path& path,
    const std::string& originalSize,
    const std::string& compressedSize
)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    output
        << "SFV-METADATA-VALIDATION\n"
        << "sample.txt\n"
        << "/legacy/sample.txt\n"
        << "/legacy/sample.txt.gz\n"
        << originalSize << '\n'
        << compressedSize << '\n'
        << "95.0\n"
        << "1730000000\n";

    output.close();
    return !output.fail();
}

static bool rejected(
    const fs::path& root,
    const std::string& name,
    const std::string& originalSize,
    const std::string& compressedSize
)
{
    const fs::path path = root / (name + ".meta");
    if (!writeMetadata(path, originalSize, compressedSize)) {
        std::cerr << "FAIL: could not write metadata fixture " << name << '\n';
        return false;
    }

    ArchiveMetadata metadata {};
    return !loadArchiveMetadata(path.string(), metadata);
}

static bool accepted(
    const fs::path& root,
    const std::string& name,
    const std::string& originalSize,
    const std::string& compressedSize
)
{
    const fs::path path = root / (name + ".meta");
    if (!writeMetadata(path, originalSize, compressedSize)) {
        return false;
    }

    ArchiveMetadata metadata {};
    return loadArchiveMetadata(path.string(), metadata) &&
        metadata.originalSize == 5 &&
        metadata.compressedSize == 3;
}

int main()
{
    const fs::path root = createTestDirectory();
    if (root.empty()) {
        std::cerr << "FAIL: could not create isolated test directory\n";
        return 1;
    }

    bool passed = true;
    const std::string overflow(128, '9');

    const struct {
        const char* name;
        const std::string original;
        const std::string compressed;
    } invalidCases[] = {
        {"negative-original", "-1", "3"},
        {"signed-original", "+5", "3"},
        {"leading-space-original", " 5", "3"},
        {"trailing-space-original", "5 ", "3"},
        {"leading-tab-original", "\t5", "3"},
        {"trailing-tab-original", "5\t", "3"},
        {"carriage-return-original", "5\r", "3"},
        {"empty-original", "", "3"},
        {"overflow-original", overflow, "3"},
        {"negative-compressed", "5", "-1"},
        {"signed-compressed", "5", "+3"},
        {"leading-space-compressed", "5", " 3"},
        {"trailing-space-compressed", "5", "3 "},
        {"leading-tab-compressed", "5", "\t3"},
        {"trailing-tab-compressed", "5", "3\t"},
        {"carriage-return-compressed", "5", "3\r"},
        {"empty-compressed", "5", ""},
        {"overflow-compressed", "5", overflow}
    };

    for (const auto& test : invalidCases) {
        const bool casePassed =
            rejected(root, test.name, test.original, test.compressed);
        std::cout
            << (casePassed ? "PASS: " : "FAIL: ")
            << test.name << '\n';
        passed = casePassed && passed;
    }

    const bool validPassed =
        accepted(root, "valid-decimal-sizes", "5", "3");
    std::cout
        << (validPassed ? "PASS: " : "FAIL: ")
        << "valid ASCII decimal sizes accepted\n";
    passed = validPassed && passed;

    if (passed) {
        std::error_code cleanupError;
        fs::remove_all(root, cleanupError);
        if (cleanupError) {
            std::cerr << "FAIL: cleanup failed: "
                      << cleanupError.message() << '\n';
            passed = false;
        }
    }

    if (!passed) {
        std::cerr
            << "METADATA VALIDATION TESTS FAILED; preserving fixtures at "
            << root.string() << '\n';
        return 1;
    }

    std::cout << "ALL METADATA VALIDATION TESTS PASSED\n";
    return 0;
}

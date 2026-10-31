
#include "compression.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

struct TempCleanup {
    fs::path root;

    ~TempCleanup() {
        std::error_code error;
        fs::remove_all(root, error);
    }
};

int main()
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    const fs::path root =
        fs::temp_directory_path() /
        ("sfv_decompression_failure_" +
         std::to_string(stamp));

    TempCleanup cleanup{root};

    std::error_code error;
    fs::create_directories(root, error);

    if (error) {
        std::cerr << "FAIL: Cannot create temporary directory.\n";
        return 1;
    }

    const fs::path original = root / "original.txt";
    const fs::path compressed = root / "corrupt.gz";
    const fs::path output = root / "partial_output.txt";

    {
        std::ofstream file(original, std::ios::binary);

        if (!file) {
            std::cerr << "FAIL: Cannot create test input.\n";
            return 1;
        }

        for (int i = 0; i < 100000; ++i) {
            file << "record-" << i
                 << ":abcdefghijklmnopqrstuvwxyz0123456789\n";
        }

        if (!file.good()) {
            std::cerr << "FAIL: Cannot write test input.\n";
            return 1;
        }
    }

    if (!compressFile(original.string(), compressed.string())) {
        std::cerr << "FAIL: Initial compression failed.\n";
        return 1;
    }

    const auto compressedSize = fs::file_size(compressed, error);

    if (error || compressedSize < 32) {
        std::cerr << "FAIL: Compressed test file is too small.\n";
        return 1;
    }

    // Truncate the compressed stream so decompression must fail.
    fs::resize_file(compressed, compressedSize / 2, error);

    if (error) {
        std::cerr << "FAIL: Could not corrupt test archive.\n";
        return 1;
    }

    const auto originalSize = fs::file_size(original, error);

    if (error) {
        std::cerr << "FAIL: Could not inspect original file size.\n";
        return 1;
    }

    const bool restored =
        decompressFile(
            compressed.string(),
            output.string(),
            originalSize
        );

    if (restored) {
        std::cerr
            << "FAIL: Corrupted archive was accepted.\n";
        return 1;
    }

    std::cout << "PASS: Corrupted archive was rejected.\n";

    if (fs::exists(output)) {
        std::cerr
            << "FAIL: Partial output file was left behind.\n";
        return 1;
    }

    std::cout
        << "PASS: Partial output file was removed.\n"
        << "Summary: ALL TESTS PASSED\n";

    return 0;
}

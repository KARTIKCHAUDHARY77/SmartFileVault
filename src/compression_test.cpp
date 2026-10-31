#include "compression.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

bool filesAreIdentical(
    const fs::path& firstPath,
    const fs::path& secondPath
) {
    std::ifstream first(firstPath, std::ios::binary);
    std::ifstream second(secondPath, std::ios::binary);

    if (!first || !second) {
        return false;
    }

    std::array<char, 8192> firstBuffer{};
    std::array<char, 8192> secondBuffer{};

    while (true) {
        first.read(firstBuffer.data(), firstBuffer.size());
        second.read(secondBuffer.data(), secondBuffer.size());

        const std::streamsize firstCount = first.gcount();
        const std::streamsize secondCount = second.gcount();

        if (firstCount != secondCount) {
            return false;
        }

        for (std::streamsize i = 0; i < firstCount; ++i) {
            if (firstBuffer[static_cast<std::size_t>(i)] !=
                secondBuffer[static_cast<std::size_t>(i)]) {
                return false;
            }
        }

        if (first.eof() && second.eof()) {
            return true;
        }

        if (first.bad() || second.bad()) {
            return false;
        }
    }
}

int main() {
    std::error_code error;

    const fs::path tempRoot = fs::temp_directory_path(error);
    if (error) {
        std::cerr << "FAIL: Cannot locate temporary directory: "
                  << error.message() << '\n';
        return 1;
    }

    const auto uniqueId =
        std::chrono::steady_clock::now().time_since_epoch().count();

    const fs::path testDirectory =
        tempRoot / ("smartfilevault_compression_test_" +
                    std::to_string(uniqueId));

    if (!fs::create_directory(testDirectory, error) || error) {
        std::cerr << "FAIL: Cannot create isolated test directory: "
                  << error.message() << '\n';
        return 1;
    }

    const fs::path original = testDirectory / "compression_test.txt";
    const fs::path compressed = testDirectory / "compression_test.gz";
    const fs::path restored = testDirectory / "compression_restored.txt";

    // Preserve this isolated directory if any test step fails.
    auto fail = [&](const std::string& message) {
        std::cerr << "FAIL: " << message << '\n';
        std::cerr << "Test files preserved at: "
                  << testDirectory << '\n';
        return 1;
    };

    // Create synthetic test data inside the isolated directory.
    {
        std::ofstream file(original, std::ios::binary);

        if (!file) {
            return fail("Could not create test input.");
        }

        for (int i = 0; i < 10000; ++i) {
            file << "SmartFileVault compression test data.\n";
        }

        file.close();

        if (!file) {
            return fail("Could not finish writing test input.");
        }
    }

    std::cout << "Original size: "
              << fs::file_size(original) << " bytes\n";

    if (!compressFile(original.string(), compressed.string())) {
        return fail("Compression failed.");
    }

    std::cout << "Compressed size: "
              << fs::file_size(compressed) << " bytes\n";

    if (compressionProvidesBenefit(
            original.string(),
            compressed.string())) {
        std::cout << "Storage benefit: YES\n";
        std::cout << "Storage saved: "
                  << getCompressionPercentage(
                         original.string(),
                         compressed.string())
                  << "%\n";
    } else {
        std::cout << "Storage benefit: NO\n";
    }

    if (!decompressFile(compressed.string(), restored.string())) {
        return fail("Decompression failed.");
    }

    std::cout << "Decompression completed.\n";
    std::cout << "Original size: "
              << fs::file_size(original) << " bytes\n";
    std::cout << "Restored size: "
              << fs::file_size(restored) << " bytes\n";

    if (!filesAreIdentical(original, restored)) {
        return fail("Byte-for-byte restoration check failed.");
    }

    std::cout << "Byte-for-byte restoration check: PASS\n";

    // Cleanup is limited to this test's unique temporary directory.
    fs::remove_all(testDirectory, error);

    if (error) {
        std::cerr << "WARNING: Tests passed, but temporary files "
                  << "could not be removed: " << error.message() << '\n';
        std::cerr << "Temporary directory: " << testDirectory << '\n';
        return 1;
    }

    std::cout << "Temporary test files cleaned up.\n";
    return 0;
}

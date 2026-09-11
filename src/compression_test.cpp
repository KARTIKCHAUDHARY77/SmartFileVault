#include "compression.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main() {

    const std::string original =
        "compression_test.txt";

    const std::string compressed =
        "compression_test.gz";

    const std::string restored =
        "compression_restored.txt";

    // Create repetitive test data.
    {
        std::ofstream file(
            original,
            std::ios::binary
        );

        for (int i = 0; i < 10000; ++i) {
            file << "SmartFileVault compression test data.\n";
        }
    }

    std::cout << "Original size: "
              << fs::file_size(original)
              << " bytes\n";

    // Compress.
    if (!compressFile(
            original,
            compressed
        )) {

        std::cout << "Compression failed.\n";
        return 1;
    }

    std::cout << "Compressed size: "
              << fs::file_size(compressed)
              << " bytes\n";

    // Check whether compression saved space.
    if (compressionProvidesBenefit(
            original,
            compressed
        )) {

        std::cout
            << "Compression provides storage benefit.\n";
    }
    else {

        std::cout
            << "Compression provides no storage benefit.\n";
    }

    // Decompress.
    if (!decompressFile(
            compressed,
            restored
        )) {

        std::cout << "Decompression failed.\n";
        return 1;
    }

    std::cout
        << "Decompression completed.\n";

    // Compare restored file size.
    if (fs::file_size(original) ==
        fs::file_size(restored)) {

        std::cout
            << "Restored file size matches original.\n";
    }
    else {

        std::cout
            << "Restored file size does not match original.\n";
    }

    // Clean test files.
    fs::remove(original);
    fs::remove(compressed);
    fs::remove(restored);

    return 0;
}
#include "compression.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main() {

    const std::string original =
        "compression_test.txt";

    const std::string compressed =
        "compression_test.gz";

    const std::string restored =
        "compression_restored.txt";

    // Create test data.
    {
        std::ofstream file(
            original,
            std::ios::binary
        );

        for (int i = 0; i < 10000; ++i) {
            file << "SmartFileVault compression test data.\n";
        }
    }

    std::cout
        << "Original size: "
        << fs::file_size(original)
        << " bytes\n";

    if (!compressFile(
            original,
            compressed
        )) {

        std::cout
            << "Compression failed.\n";

        return 1;
    }

    std::cout
        << "Compressed size: "
        << fs::file_size(compressed)
        << " bytes\n";

    if (compressionProvidesBenefit(
            original,
            compressed
        )) {

        std::cout
            << "Storage benefit: YES\n";

        std::cout
            << "Storage saved: "
            << getCompressionPercentage(
                   original,
                   compressed
               )
            << "%\n";
    }
    else {

        std::cout
            << "Storage benefit: NO\n";
    }

    if (!decompressFile(
            compressed,
            restored
        )) {

        std::cout
            << "Decompression failed.\n";

        fs::remove(original);
        fs::remove(compressed);

        return 1;
    }

    std::cout
        << "Decompression completed.\n";

    std::cout
        << "Original size: "
        << fs::file_size(original)
        << " bytes\n";

    std::cout
        << "Restored size: "
        << fs::file_size(restored)
        << " bytes\n";

    if (fs::file_size(original) ==
        fs::file_size(restored)) {

        std::cout
            << "Restoration size check: PASS\n";
    }
    else {

        std::cout
            << "Restoration size check: FAIL\n";
    }

    fs::remove(original);
    fs::remove(compressed);
    fs::remove(restored);

    return 0;
}
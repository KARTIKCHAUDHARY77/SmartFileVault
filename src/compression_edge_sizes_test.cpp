#include "compression.h"
#include "test_support.h"

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Compress, decompress and compare byte-for-byte.
static bool roundTrip(
    const testsupport::TestDirectory& dir,
    const std::string& label,
    const std::vector<unsigned char>& bytes
) {
    const fs::path source = dir.path() / (label + ".bin");
    const fs::path packed = dir.path() / (label + ".gz");
    const fs::path restored = dir.path() / (label + ".restored");

    std::vector<unsigned char> back;

    return testsupport::writeAllBytes(source, bytes) &&
           compressFile(source.string(), packed.string()) &&
           decompressFile(packed.string(), restored.string()) &&
           testsupport::readAllBytes(restored, back) &&
           back == bytes;
}

int main() {
    testsupport::TestDirectory dir("smartfilevault_edge_sizes_test");

    if (!dir.ok()) {
        std::cerr << "FAIL: Could not create test directory\n";
        return 1;
    }

    // Sizes around the 8192-byte read buffer, plus empty and tiny files.
    const std::vector<std::size_t> sizes = {
        0, 1, 2, 8191, 8192, 8193, 16384, 16385, 100000
    };

    int failures = 0;

    for (const std::size_t size : sizes) {
        const std::string random = "random_" + std::to_string(size);
        const std::string pattern = "pattern_" + std::to_string(size);

        const bool randomOk = roundTrip(
            dir, random, testsupport::pseudoRandomBytes(size, 7));
        const bool patternOk = roundTrip(
            dir, pattern, testsupport::repeatedPatternBytes(size));

        std::cout << (randomOk ? "PASS: " : "FAIL: ")
                  << "round trip, random data, " << size << " bytes\n";
        std::cout << (patternOk ? "PASS: " : "FAIL: ")
                  << "round trip, pattern data, " << size << " bytes\n";

        if (!randomOk) ++failures;
        if (!patternOk) ++failures;
    }

    if (failures != 0) {
        std::cerr << "Edge-size tests failed: " << failures << '\n';
        return 1;
    }

    if (!dir.cleanup()) {
        return 1;
    }

    std::cout << "All edge-size tests passed.\n";
    return 0;
}

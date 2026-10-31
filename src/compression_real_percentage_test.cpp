#include "compression.h"
#include "test_support.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

static bool getSize(const fs::path& path, std::uintmax_t& size) {
    std::error_code error;
    size = fs::file_size(path, error);
    return !error;
}

int main() {
    testsupport::TestDirectory dir("smartfilevault_real_percentage_test");

    if (!dir.ok()) {
        std::cerr << "FAIL: Could not create test directory\n";
        return 1;
    }

    int failures = 0;

    // Case 1: incompressible data must report exactly 0% savings.
    {
        const fs::path original = dir.path() / "random.bin";
        const fs::path compressed = dir.path() / "random.bin.gz";

        if (!testsupport::writeAllBytes(
                original,
                testsupport::pseudoRandomBytes(100000, 12345))) {
            std::cerr << "FAIL: Could not create random test file\n";
            return 1;
        }

        if (!compressFile(original.string(), compressed.string())) {
            std::cerr << "FAIL: Compression of random data failed\n";
            return 1;
        }

        std::uintmax_t originalSize = 0;
        std::uintmax_t compressedSize = 0;

        if (!getSize(original, originalSize) ||
            !getSize(compressed, compressedSize)) {
            std::cerr << "FAIL: Could not read file sizes (random case)\n";
            return 1;
        }

        const double percentage = getCompressionPercentage(
            original.string(), compressed.string());

        std::cout << "[random] Original size: " << originalSize
                  << " bytes\n";
        std::cout << "[random] Compressed size: " << compressedSize
                  << " bytes\n";
        std::cout << "[random] Reported savings: " << percentage << "%\n";

        if (compressedSize < originalSize) {
            std::cerr << "FAIL: Random test data unexpectedly compressed;"
                         " the no-benefit case was not exercised\n";
            ++failures;
        } else if (percentage == 0.0) {
            std::cout << "PASS: No-benefit compression reports 0%\n";
        } else {
            std::cerr << "FAIL: No-benefit compression reported "
                      << percentage << "% instead of 0%\n";
            ++failures;
        }
    }

    // Case 2: compressible data must match the savings formula.
    {
        const fs::path original = dir.path() / "pattern.bin";
        const fs::path compressed = dir.path() / "pattern.bin.gz";

        if (!testsupport::writeAllBytes(
                original,
                testsupport::repeatedPatternBytes(100000))) {
            std::cerr << "FAIL: Could not create pattern test file\n";
            return 1;
        }

        if (!compressFile(original.string(), compressed.string())) {
            std::cerr << "FAIL: Compression of pattern data failed\n";
            return 1;
        }

        std::uintmax_t originalSize = 0;
        std::uintmax_t compressedSize = 0;

        if (!getSize(original, originalSize) ||
            !getSize(compressed, compressedSize)) {
            std::cerr << "FAIL: Could not read file sizes (pattern case)\n";
            return 1;
        }

        const double percentage = getCompressionPercentage(
            original.string(), compressed.string());

        std::cout << "[pattern] Original size: " << originalSize
                  << " bytes\n";
        std::cout << "[pattern] Compressed size: " << compressedSize
                  << " bytes\n";
        std::cout << "[pattern] Reported savings: " << percentage << "%\n";

        if (compressedSize >= originalSize) {
            std::cerr << "FAIL: Compressible data did not shrink\n";
            ++failures;
        } else {
            const double expected =
                (static_cast<double>(originalSize) -
                 static_cast<double>(compressedSize)) /
                static_cast<double>(originalSize) * 100.0;

            // Tolerance allows for the function rounding its result.
            if (std::fabs(percentage - expected) < 0.01 &&
                percentage > 0.0 && percentage < 100.0) {
                std::cout << "PASS: Savings match formula (expected "
                          << expected << "%)\n";
            } else {
                std::cerr << "FAIL: Reported " << percentage
                          << "% but formula gives " << expected << "%\n";
                ++failures;
            }
        }
    }

    if (failures != 0) {
        std::cerr << "Percentage tests failed: " << failures << '\n';
        return 1;
    }

    if (!dir.cleanup()) {
        return 1;
    }

    std::cout << "Percentage test passed.\n";
    return 0;
}
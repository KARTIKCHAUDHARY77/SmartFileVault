#include "compression.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

static bool writeSizedFile(
    const fs::path& path,
    std::size_t size,
    char value
) {
    std::ofstream file(
        path,
        std::ios::binary | std::ios::trunc
    );

    if (!file) {
        return false;
    }

    std::string data(size, value);
    file.write(
        data.data(),
        static_cast<std::streamsize>(data.size())
    );

    return file.good();
}

static bool checkCase(
    const std::string& name,
    std::size_t originalSize,
    std::size_t compressedSize,
    double expected
) {
    const fs::path original = "percentage_original.bin";
    const fs::path candidate = "percentage_candidate.bin";

    if (!writeSizedFile(original, originalSize, 'A') ||
        !writeSizedFile(candidate, compressedSize, 'B')) {
        std::cerr << "FAIL: Could not create test files\n";
        return false;
    }

    const double actual = getCompressionPercentage(
        original.string(),
        candidate.string()
    );

    if (!std::isfinite(actual) ||
        std::abs(actual - expected) > 0.000001) {
        std::cerr << "FAIL: " << name
                  << ", expected " << expected
                  << "% but got " << actual << "%\n";
        return false;
    }

    std::cout << "PASS: " << name
              << " (" << actual << "%)\n";
    return true;
}

int main() {
    bool passed = true;

    passed &= checkCase("Storage saving", 100, 80, 20.0);
    passed &= checkCase("No size change", 100, 100, 0.0);
    passed &= checkCase("Storage expansion", 100, 120, -20.0);
    passed &= checkCase("Empty original file", 0, 0, 0.0);

    std::error_code error;
    fs::remove("percentage_original.bin", error);
    fs::remove("percentage_candidate.bin", error);

    if (!passed) {
        return 1;
    }

    std::cout << "ALL PERCENTAGE TESTS PASSED\n";
    return 0;
}

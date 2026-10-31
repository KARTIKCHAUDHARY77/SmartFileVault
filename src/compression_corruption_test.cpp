#include "compression.h"
#include "test_support.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

static bool checkRejected(
    const fs::path& compressedFile,
    const fs::path& restoredFile,
    const std::string& testName
) {
    std::error_code error;

    const bool decompressed = decompressFile(
        compressedFile.string(),
        restoredFile.string()
    );

    const bool restoredExists = fs::exists(restoredFile, error);

    const bool passed =
        !decompressed && !restoredExists && !error;

    if (passed) {
        std::cout << "PASS: " << testName << '\n';
    } else {
        std::cerr << "FAIL: " << testName << '\n';
    }

    return passed;
}

struct CorruptionCase {
    std::string label;
    std::string fileName;
    std::vector<unsigned char> bytes;
};

int main() {
    testsupport::TestDirectory dir("smartfilevault_corruption_test");

    if (!dir.ok()) {
        std::cerr << "FAIL: Could not create test directory\n";
        return 1;
    }

    const fs::path testDirectory = dir.path();
    const fs::path originalFile = testDirectory / "original.txt";
    const fs::path validGzip = testDirectory / "valid.gz";
    const fs::path validRestored = testDirectory / "valid_restored.txt";

    int failures = 0;

    // Compressible multi-line text so the setup does not depend on
    // how compressFile treats tiny files.
    std::string text;
    for (int i = 0; i < 50; ++i) {
        text += "SmartFileVault corruption verification line " +
                std::to_string(i) + ".\n";
    }

    const std::vector<unsigned char> originalBytes(
        text.begin(), text.end());

    if (!testsupport::writeAllBytes(originalFile, originalBytes)) {
        std::cerr << "FAIL: Could not create original file\n";
        return 1;
    }

    if (!compressFile(originalFile.string(), validGzip.string())) {
        std::cerr << "FAIL: Could not create valid GZIP file\n";
        return 1;
    }

    // Positive control: a valid file must decompress exactly.
    // This proves the rejection tests below are not passing only
    // because decompressFile always fails.
    {
        std::vector<unsigned char> restoredBytes;

        const bool decompressed = decompressFile(
            validGzip.string(),
            validRestored.string()
        );

        if (decompressed &&
            testsupport::readAllBytes(validRestored, restoredBytes) &&
            restoredBytes == originalBytes) {
            std::cout << "PASS: Valid GZIP restored byte-for-byte\n";
        } else {
            std::cerr << "FAIL: Valid GZIP did not restore correctly\n";
            ++failures;
        }
    }

    std::vector<unsigned char> data;

    if (!testsupport::readAllBytes(validGzip, data)) {
        std::cerr << "FAIL: Could not read valid GZIP file\n";
        return 1;
    }

    // 10-byte header + 8-byte footer minimum.
    if (data.size() < 18) {
        std::cerr << "FAIL: Generated GZIP file is unexpectedly small\n";
        return 1;
    }

    std::vector<CorruptionCase> cases;

    {
        const std::string notGzip = "This is not a valid GZIP file.";
        cases.push_back({
            "Non-GZIP input rejected without partial output",
            "invalid.gz",
            std::vector<unsigned char>(notGzip.begin(), notGzip.end())
        });
    }

    {
        // Footer = CRC32 (4 bytes) + original size (4 bytes).
        std::vector<unsigned char> corrupted = data;
        corrupted[corrupted.size() - 8] ^= 0xFF;
        cases.push_back({
            "Corrupted GZIP checksum rejected without partial output",
            "corrupted_crc.gz",
            corrupted
        });
    }

    cases.push_back({
        "GZIP missing last 4 bytes rejected without partial output",
        "truncated_minus4.gz",
        std::vector<unsigned char>(data.begin(), data.end() - 4)
    });

    cases.push_back({
        "GZIP missing last 1 byte rejected without partial output",
        "truncated_minus1.gz",
        std::vector<unsigned char>(data.begin(), data.end() - 1)
    });

    cases.push_back({
        "GZIP cut in half rejected without partial output",
        "truncated_half.gz",
        std::vector<unsigned char>(
            data.begin(),
            data.begin() + static_cast<std::ptrdiff_t>(data.size() / 2))
    });

    cases.push_back({
        "GZIP header only rejected without partial output",
        "truncated_header_only.gz",
        std::vector<unsigned char>(data.begin(), data.begin() + 10)
    });
        {
        std::vector<unsigned char> trailing = data;
        const std::string junk = "TRAILING-JUNK";
        trailing.insert(trailing.end(), junk.begin(), junk.end());

        cases.push_back({
            "GZIP with trailing junk rejected without partial output",
            "trailing_junk.gz",
            trailing
        });
    }
    int index = 0;

    for (const CorruptionCase& testCase : cases) {
        const fs::path inputFile = testDirectory / testCase.fileName;

        // Separate restore path per case so one bad result
        // cannot cascade into the next case.
        const fs::path restoredFile =
            testDirectory /
            ("restored_" + std::to_string(index++) + ".txt");

        if (!testsupport::writeAllBytes(inputFile, testCase.bytes)) {
            std::cerr << "FAIL: Could not create " << testCase.fileName
                      << '\n';
            return 1;
        }

        if (!checkRejected(inputFile, restoredFile, testCase.label)) {
            ++failures;
        }
    }

    if (failures != 0) {
        std::cerr << "Corruption tests failed: " << failures << '\n';
        return 1;
    }

    if (!dir.cleanup()) {
        return 1;
    }

    std::cout << "All corruption tests passed.\n";
    return 0;
}
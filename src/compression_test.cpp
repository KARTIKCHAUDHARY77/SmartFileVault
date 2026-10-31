
#include "compression.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

// Automatically removes only the unique temporary directory
// created by this test. It never targets repository files.
struct TemporaryDirectory {
    fs::path path;

    bool cleanup() {
        if (path.empty()) {
            return true;
        }

        std::error_code error;
        fs::remove_all(path, error);

        if (error) {
            std::cerr
                << "FAIL: Could not clean temporary directory: "
                << error.message() << '\n';
            return false;
        }

        path.clear();
        return true;
    }

    ~TemporaryDirectory() {
        if (!path.empty()) {
            std::error_code error;
            fs::remove_all(path, error);

            if (error) {
                std::cerr
                    << "WARNING: Temporary cleanup failed: "
                    << error.message() << '\n';
            }
        }
    }
};

static fs::path createTemporaryDirectory() {
    std::error_code error;
    const fs::path tempRoot =
        fs::temp_directory_path(error);

    if (error) {
        std::cerr
            << "FAIL: Cannot locate temporary directory: "
            << error.message() << '\n';
        return {};
    }

    const auto timestamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        const fs::path candidate =
            tempRoot /
            ("sfv_compression_roundtrip_" +
             std::to_string(timestamp) + "_" +
             std::to_string(attempt));

        error.clear();

        if (fs::create_directory(candidate, error)) {
            return candidate;
        }

        if (error && error != std::errc::file_exists) {
            std::cerr
                << "FAIL: Cannot create temporary directory: "
                << error.message() << '\n';
            return {};
        }
    }

    std::cerr
        << "FAIL: Could not create a unique temporary directory.\n";

    return {};
}

static bool writeBinaryFile(
    const fs::path& path,
    const std::string& contents
) {
    std::ofstream output(
        path,
        std::ios::binary | std::ios::trunc
    );

    if (!output) {
        return false;
    }

    if (!contents.empty()) {
        output.write(
            contents.data(),
            static_cast<std::streamsize>(contents.size())
        );
    }

    output.close();

    return !output.fail();
}

static bool readBinaryFile(
    const fs::path& path,
    std::string& contents
) {
    std::ifstream input(
        path,
        std::ios::binary
    );

    if (!input) {
        return false;
    }

    contents.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    );

    return !input.bad();
}

static std::string makeCompressibleText() {
    std::string contents;

    const std::string line =
        "SmartFileVault lossless compression regression data.\n";

    contents.reserve(600000);

    for (int i = 0; i < 12000; ++i) {
        contents += line;
    }

    return contents;
}

static std::string makeCompressibleBinary() {
    // Include null bytes and values above 0x7F to ensure
    // the compression path handles arbitrary binary data.
    const unsigned char pattern[] = {
        0x00, 0xFF, 0x80, 0x00,
        0x53, 0x56, 0x46, 0x7F
    };

    std::string contents;
    contents.reserve(8U * 16384U);

    for (int repetition = 0; repetition < 16384; ++repetition) {
        for (unsigned char value : pattern) {
            contents.push_back(static_cast<char>(value));
        }
    }

    return contents;
}

static std::string makeHighEntropyBinary() {
    // A fixed seed makes the test reproducible. This synthetic
    // high-entropy payload should not benefit from gzip compression.
    std::mt19937 generator(0x5F17U);

    std::string contents;
    contents.reserve(128U * 1024U);

    for (std::size_t i = 0; i < 128U * 1024U; ++i) {
        contents.push_back(
            static_cast<char>(generator() & 0xFFU)
        );
    }

    return contents;
}

static bool runRoundTripCase(
    const fs::path& testDirectory,
    const std::string& description,
    const std::string& filename,
    const std::string& payload,
    bool expectedStorageBenefit
) {
    const fs::path original =
        testDirectory / filename;

    const fs::path compressed =
        testDirectory / (filename + ".gz");

    const fs::path restored =
        testDirectory / (filename + ".restored");

    std::cout
        << "\n========== " << description
        << " ==========\n";

    if (!writeBinaryFile(original, payload)) {
        std::cerr
            << "FAIL: Could not create original test file.\n";
        return false;
    }

    std::error_code sizeError;

    const std::uintmax_t originalSize =
        fs::file_size(original, sizeError);

    if (sizeError) {
        std::cerr
            << "FAIL: Could not inspect original file size: "
            << sizeError.message() << '\n';
        return false;
    }

    std::cout
        << "Original size: " << originalSize << " bytes\n";

    if (!compressFile(original.string(), compressed.string())) {
        std::cerr
            << "FAIL: Compression failed for "
            << filename << '\n';
        return false;
    }

    sizeError.clear();

    const std::uintmax_t compressedSize =
        fs::file_size(compressed, sizeError);

    if (sizeError) {
        std::cerr
            << "FAIL: Could not inspect compressed file size: "
            << sizeError.message() << '\n';
        return false;
    }

    std::cout
        << "Compressed size: "
        << compressedSize << " bytes\n";

    const bool actualStorageBenefit =
        compressionProvidesBenefit(
            original.string(),
            compressed.string()
        );

    const double percentage =
        getCompressionPercentage(
            original.string(),
            compressed.string()
        );

    std::cout
        << "Storage benefit: "
        << (actualStorageBenefit ? "YES" : "NO")
        << '\n';

    std::cout
        << "Storage saved: "
        << percentage << "%\n";

    bool passed = true;

    const bool expectedSizeRelationship =
        expectedStorageBenefit
            ? compressedSize < originalSize
            : compressedSize >= originalSize;

    if (actualStorageBenefit != expectedStorageBenefit ||
        !expectedSizeRelationship) {
        std::cerr
            << "FAIL: Unexpected storage-benefit result for "
            << filename << '\n';
        passed = false;
    } else {
        std::cout
            << "PASS: Storage-benefit decision\n";
    }

    if (!std::isfinite(percentage)) {
        std::cerr
            << "FAIL: Compression percentage is not finite.\n";
        passed = false;
    } else if (expectedStorageBenefit && percentage <= 0.0) {
        std::cerr
            << "FAIL: Expected a positive storage saving.\n";
        passed = false;
    } else if (!expectedStorageBenefit && percentage > 0.0) {
        std::cerr
            << "FAIL: Unexpected positive storage saving.\n";
        passed = false;
    } else {
        std::cout
            << "PASS: Compression percentage sign\n";
    }

    if (!decompressFile(
            compressed.string(),
            restored.string(),
            originalSize
        )) {
        std::cerr
            << "FAIL: Decompression failed for "
            << filename << '\n';
        return false;
    }

    std::string originalBytes;
    std::string restoredBytes;

    if (!readBinaryFile(original, originalBytes)) {
        std::cerr
            << "FAIL: Could not read original file for comparison.\n";
        return false;
    }

    if (!readBinaryFile(restored, restoredBytes)) {
        std::cerr
            << "FAIL: Could not read restored file for comparison.\n";
        return false;
    }

    if (originalBytes != payload) {
        std::cerr
            << "FAIL: Original fixture differs from expected bytes.\n";
        passed = false;
    }

    // Compare every byte, not only the file sizes.
    if (restoredBytes != originalBytes) {
        std::cerr
            << "FAIL: Restored bytes differ from original bytes.\n"
            << "Original bytes: " << originalBytes.size() << '\n'
            << "Restored bytes: " << restoredBytes.size() << '\n';

        passed = false;
    } else {
        std::cout
            << "PASS: Exact byte-for-byte restoration ("
            << restoredBytes.size() << " bytes)\n";
    }

    if (passed) {
        std::cout
            << "PASS: " << description << '\n';
    }

    return passed;
}

int main() {
    TemporaryDirectory temporary{
        createTemporaryDirectory()
    };

    if (temporary.path.empty()) {
        std::cerr
            << "COMPRESSION ROUND-TRIP TESTS FAILED\n";
        return 1;
    }

    const std::string textPayload =
        makeCompressibleText();

    const std::string binaryPayload =
        makeCompressibleBinary();

    const std::string highEntropyPayload =
        makeHighEntropyBinary();

    bool passed = true;

    // Different extensions and extensionless filenames use
    // exactly the same lossless compression interface.
    passed = runRoundTripCase(
        temporary.path,
        "Text file (.txt)",
        "document.txt",
        textPayload,
        true
    ) && passed;

    passed = runRoundTripCase(
        temporary.path,
        "Binary file (.bin)",
        "binary_payload.bin",
        binaryPayload,
        true
    ) && passed;

    passed = runRoundTripCase(
        temporary.path,
        "Unknown extension (.custom)",
        "payload.custom",
        textPayload,
        true
    ) && passed;

    passed = runRoundTripCase(
        temporary.path,
        "Extensionless file",
        "extensionless",
        binaryPayload,
        true
    ) && passed;

    passed = runRoundTripCase(
        temporary.path,
        "High-entropy binary with no expected storage benefit",
        "high_entropy.bin",
        highEntropyPayload,
        false
    ) && passed;

    // A successful low-level round trip does not itself mean
    // the file should be archived. Archive policy must still
    // reject candidates that do not save storage space.
    if (!temporary.cleanup()) {
        passed = false;
    }

    std::cout << "\n============================================\n";

    if (!passed) {
        std::cerr
            << "COMPRESSION ROUND-TRIP TESTS FAILED\n";
        return 1;
    }

    std::cout
        << "ALL COMPRESSION ROUND-TRIP TESTS PASSED\n";

    return 0;
}

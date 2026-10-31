#include "compression.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

struct TemporaryDirectory {
    fs::path path;

    ~TemporaryDirectory() {
        if (!path.empty()) {
            std::error_code error;
            fs::remove_all(path, error);

            if (error) {
                std::cerr << "WARNING: temporary cleanup failed: "
                          << error.message() << '\n';
            }
        }
    }
};

static fs::path createTemporaryDirectory()
{
    std::error_code error;
    const fs::path tempRoot = fs::temp_directory_path(error);

    if (error) {
        std::cerr << "FAIL: cannot locate temporary directory: "
                  << error.message() << '\n';
        return {};
    }

    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch().count();

    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        const fs::path candidate =
            tempRoot /
            ("sfv_output_safety_" + std::to_string(stamp) +
             "_" + std::to_string(attempt));

        error.clear();

        if (fs::create_directory(candidate, error)) {
            return candidate;
        }

        if (error && error != std::errc::file_exists) {
            std::cerr << "FAIL: cannot create temporary directory: "
                      << error.message() << '\n';
            return {};
        }
    }

    std::cerr << "FAIL: could not create a unique temporary directory.\n";
    return {};
}

static bool writeFile(
    const fs::path& path,
    const std::string& contents
)
{
    std::ofstream output(path, std::ios::binary);

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

static bool readFile(
    const fs::path& path,
    std::string& contents
)
{
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        return false;
    }

    contents.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    );

    return !input.bad();
}

static bool check(bool passed, const char* description)
{
    std::cout << (passed ? "PASS: " : "FAIL: ")
              << description << '\n';
    return passed;
}

static bool hasTemporaryOutput(const fs::path& root)
{
    std::error_code error;

    for (fs::directory_iterator it(root, error);
         !error && it != fs::directory_iterator();
         it.increment(error)) {
        if (it->path().filename().string().rfind(
                ".sfv-output-", 0
            ) == 0) {
            return true;
        }
    }

    return static_cast<bool>(error);
}

int main()
{
    TemporaryDirectory temporary{createTemporaryDirectory()};

    if (temporary.path.empty()) {
        return 1;
    }

    const fs::path input = temporary.path / "source.bin";
    const fs::path existingCompressed =
        temporary.path / "existing.gz";
    const fs::path archive =
        temporary.path / "valid.gz";
    const fs::path existingRestored =
        temporary.path / "existing-restored.bin";
    const fs::path restored =
        temporary.path / "restored.bin";

    const std::string payload =
        "SmartFileVault must never destroy an original or overwrite "
        "an existing destination. This payload is repeated to make "
        "a useful lossless compression fixture.\n";

    std::string originalBytes;
    std::string archiveBytes;
    std::string actualBytes;

    bool passed = true;

    if (!writeFile(input, payload)) {
        std::cerr << "FAIL: could not write source fixture.\n";
        return 1;
    }

    // Same-path compression must fail without truncating its source.
    const bool samePathCompression =
        !compressFile(input.string(), input.string()) &&
        readFile(input, actualBytes) &&
        actualBytes == payload;

    passed = check(
        samePathCompression,
        "Same-path compression is rejected and source bytes are preserved"
    ) && passed;

    const std::string compressedSentinel =
        "pre-existing compressed destination must remain unchanged";

    if (!writeFile(existingCompressed, compressedSentinel)) {
        std::cerr << "FAIL: could not write existing-output fixture.\n";
        return 1;
    }

    const bool existingCompressionDestination =
        !compressFile(input.string(), existingCompressed.string()) &&
        readFile(existingCompressed, actualBytes) &&
        actualBytes == compressedSentinel;

    passed = check(
        existingCompressionDestination,
        "Compression refuses to overwrite an existing destination"
    ) && passed;

    if (!compressFile(input.string(), archive.string())) {
        std::cerr << "FAIL: normal compression failed.\n";
        return 1;
    }

    if (!readFile(input, originalBytes) ||
        originalBytes != payload ||
        !readFile(archive, archiveBytes)) {
        std::cerr << "FAIL: could not verify successful compressed output.\n";
        return 1;
    }

    // Same-path decompression must preserve the compressed source.
    const bool samePathDecompression =
        !decompressFile(archive.string(), archive.string(), static_cast<std::uintmax_t>(payload.size())) &&
        readFile(archive, actualBytes) &&
        actualBytes == archiveBytes;

    passed = check(
        samePathDecompression,
        "Same-path decompression is rejected and archive bytes are preserved"
    ) && passed;

    const std::string restoredSentinel =
        "pre-existing restored destination must remain unchanged";

    if (!writeFile(existingRestored, restoredSentinel)) {
        std::cerr << "FAIL: could not write existing restore fixture.\n";
        return 1;
    }

    const bool existingRestoreDestination =
        !decompressFile(archive.string(), existingRestored.string(), static_cast<std::uintmax_t>(payload.size())) &&
        readFile(existingRestored, actualBytes) &&
        actualBytes == restoredSentinel;

    passed = check(
        existingRestoreDestination,
        "Decompression refuses to overwrite an existing destination"
    ) && passed;

    const bool normalRoundTrip =
        decompressFile(archive.string(), restored.string(), static_cast<std::uintmax_t>(payload.size())) &&
        readFile(restored, actualBytes) &&
        actualBytes == originalBytes;

    passed = check(
        normalRoundTrip,
        "Normal compression and decompression remain byte-exact"
    ) && passed;

    passed = check(
        !hasTemporaryOutput(temporary.path),
        "No temporary output files remain after success or rejection"
    ) && passed;

    if (!passed) {
        std::cerr << "COMPRESSION OUTPUT SAFETY TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL COMPRESSION OUTPUT SAFETY TESTS PASSED\n";
    return 0;
}

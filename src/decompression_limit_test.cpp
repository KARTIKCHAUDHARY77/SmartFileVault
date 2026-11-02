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
                std::cerr
                    << "WARNING: temporary cleanup failed: "
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
        std::cerr
            << "FAIL: cannot locate temporary directory: "
            << error.message() << '\n';
        return {};
    }

    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch().count();

    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        const fs::path candidate =
            tempRoot /
            ("sfv_decompression_limit_" +
             std::to_string(stamp) + "_" +
             std::to_string(attempt));

        error.clear();

        if (fs::create_directory(candidate, error)) {
            return candidate;
        }

        if (error && error != std::errc::file_exists) {
            std::cerr
                << "FAIL: cannot create temporary directory: "
                << error.message() << '\n';
            return {};
        }
    }

    std::cerr
        << "FAIL: could not create a unique temporary directory.\n";
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

static bool check(bool passed, const char* description)
{
    std::cout
        << (passed ? "PASS: " : "FAIL: ")
        << description << '\n';

    return passed;
}

static bool rejectedWithoutArtifacts(
    const fs::path& archive,
    const fs::path& output,
    std::uintmax_t expectedBytes,
    const fs::path& root,
    const char* description
)
{
    std::error_code error;
    fs::remove(output, error);

    if (error) {
        return check(false, description);
    }

    const bool rejected =
        !decompressFile(
            archive.string(),
            output.string(),
            expectedBytes
        );

    const bool outputAbsent =
        !fs::exists(output, error) && !error;

    const bool temporaryAbsent =
        !hasTemporaryOutput(root);

    return check(
        rejected && outputAbsent && temporaryAbsent,
        description
    );
}

int main()
{
    TemporaryDirectory temporary{createTemporaryDirectory()};

    if (temporary.path.empty()) {
        return 1;
    }

    const fs::path source = temporary.path / "source.bin";
    const fs::path validArchive = temporary.path / "valid.gz";
    const fs::path exactOutput = temporary.path / "exact-output.bin";
    const fs::path secondSource = temporary.path / "second-source.bin";
    const fs::path secondArchive = temporary.path / "second.gz";

    // A highly compressible payload exercises the output cap without
    // requiring a large fixture or excessive memory.
    const std::string payload(1024 * 1024, 'A');

    if (!writeFile(source, payload) ||
        !compressFile(source.string(), validArchive.string())) {
        std::cerr << "FAIL: could not create the valid GZIP fixture.\n";
        return 1;
    }

    std::string archiveBytes;

    if (!readFile(validArchive, archiveBytes) ||
        archiveBytes.size() < 18) {
        std::cerr << "FAIL: could not inspect the valid GZIP fixture.\n";
        return 1;
    }

    bool passed = true;

    const bool exactBoundary =
        decompressFile(
            validArchive.string(),
            exactOutput.string(),
            static_cast<std::uintmax_t>(payload.size())
        );

    std::string restoredBytes;

    passed = check(
        exactBoundary &&
        readFile(exactOutput, restoredBytes) &&
        restoredBytes == payload,
        "Output exactly at the expected-size boundary is accepted"
    ) && passed;

    passed = rejectedWithoutArtifacts(
        validArchive,
        temporary.path / "too-small.bin",
        static_cast<std::uintmax_t>(payload.size() - 1),
        temporary.path,
        "Output exceeding the expected-size limit is rejected during streaming"
    ) && passed;

    passed = rejectedWithoutArtifacts(
        validArchive,
        temporary.path / "too-large-metadata.bin",
        static_cast<std::uintmax_t>(payload.size() + 1),
        temporary.path,
        "Output shorter than the metadata's exact expected size is rejected"
    ) && passed;

    passed = rejectedWithoutArtifacts(
        validArchive,
        temporary.path / "zero-limit.bin",
        0,
        temporary.path,
        "Non-empty output is rejected when the expected size is zero"
    ) && passed;

    std::string malformedHeader = archiveBytes;
    malformedHeader[0] = 'X';
    const fs::path malformedArchive = temporary.path / "malformed-header.gz";

    if (!writeFile(malformedArchive, malformedHeader)) {
        std::cerr << "FAIL: could not create malformed-header fixture.\n";
        return 1;
    }

    passed = rejectedWithoutArtifacts(
        malformedArchive,
        temporary.path / "malformed-header-output.bin",
        static_cast<std::uintmax_t>(payload.size()),
        temporary.path,
        "Malformed GZIP header is rejected and temporary output is removed"
    ) && passed;

    std::string truncated = archiveBytes;
    truncated.resize(truncated.size() - 4);
    const fs::path truncatedArchive = temporary.path / "truncated.gz";

    if (!writeFile(truncatedArchive, truncated)) {
        std::cerr << "FAIL: could not create truncated fixture.\n";
        return 1;
    }

    passed = rejectedWithoutArtifacts(
        truncatedArchive,
        temporary.path / "truncated-output.bin",
        static_cast<std::uintmax_t>(payload.size()),
        temporary.path,
        "Truncated GZIP trailer is rejected and temporary output is removed"
    ) && passed;

    std::string badChecksum = archiveBytes;
    badChecksum[badChecksum.size() - 8] =
        static_cast<char>(badChecksum[badChecksum.size() - 8] ^ 0x01);
    const fs::path checksumArchive = temporary.path / "bad-checksum.gz";

    if (!writeFile(checksumArchive, badChecksum)) {
        std::cerr << "FAIL: could not create checksum fixture.\n";
        return 1;
    }

    passed = rejectedWithoutArtifacts(
        checksumArchive,
        temporary.path / "bad-checksum-output.bin",
        static_cast<std::uintmax_t>(payload.size()),
        temporary.path,
        "GZIP checksum corruption is rejected"
    ) && passed;

    const fs::path trailingArchive = temporary.path / "trailing-data.gz";
    std::string withTrailingData = archiveBytes + "trailing-data";

    if (!writeFile(trailingArchive, withTrailingData)) {
        std::cerr << "FAIL: could not create trailing-data fixture.\n";
        return 1;
    }

    passed = rejectedWithoutArtifacts(
        trailingArchive,
        temporary.path / "trailing-output.bin",
        static_cast<std::uintmax_t>(payload.size()),
        temporary.path,
        "Trailing bytes after the GZIP member are rejected"
    ) && passed;

    const std::string secondPayload = "second independent GZIP member";

    if (!writeFile(secondSource, secondPayload) ||
        !compressFile(secondSource.string(), secondArchive.string())) {
        std::cerr << "FAIL: could not create second GZIP member.\n";
        return 1;
    }

    std::string secondArchiveBytes;

    if (!readFile(secondArchive, secondArchiveBytes)) {
        std::cerr << "FAIL: could not read second GZIP member.\n";
        return 1;
    }

    const fs::path concatenatedArchive =
        temporary.path / "concatenated-members.gz";

    if (!writeFile(
            concatenatedArchive,
            archiveBytes + secondArchiveBytes
        )) {
        std::cerr << "FAIL: could not create concatenated-member fixture.\n";
        return 1;
    }

    passed = rejectedWithoutArtifacts(
        concatenatedArchive,
        temporary.path / "concatenated-output.bin",
        static_cast<std::uintmax_t>(payload.size()),
        temporary.path,
        "Additional concatenated GZIP members are rejected"
    ) && passed;

    const fs::path emptySource = temporary.path / "empty-source.bin";
    const fs::path emptyArchive = temporary.path / "empty.gz";
    const fs::path emptyOutput = temporary.path / "empty-output.bin";

    if (!writeFile(emptySource, "") ||
        !compressFile(emptySource.string(), emptyArchive.string())) {
        std::cerr << "FAIL: could not create empty GZIP fixture.\n";
        return 1;
    }

    const bool emptyRoundTrip =
        decompressFile(emptyArchive.string(), emptyOutput.string(), 0) &&
        fs::exists(emptyOutput) &&
        fs::file_size(emptyOutput) == 0;

    passed = check(
        emptyRoundTrip,
        "A valid empty GZIP archive is accepted with an expected size of zero"
    ) && passed;

    std::string archiveAfterRejections;

    passed = check(
        readFile(validArchive, archiveAfterRejections) &&
        archiveAfterRejections == archiveBytes,
        "Rejected decompression attempts preserve the source archive"
    ) && passed;

    passed = check(
        !hasTemporaryOutput(temporary.path),
        "No temporary output files remain after success or rejection"
    ) && passed;

    if (!passed) {
        std::cerr << "DECOMPRESSION LIMIT TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL DECOMPRESSION LIMIT TESTS PASSED\n";
    return 0;
}

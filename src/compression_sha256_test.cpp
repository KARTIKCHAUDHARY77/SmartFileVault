#include "compression.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

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
    std::string pattern =
        (fs::temp_directory_path() / "sfv-sha256-test-XXXXXX").string();
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');

    char* created = ::mkdtemp(writable.data());
    return created == nullptr ? fs::path{} : fs::path(created);
}

static bool writeFile(const fs::path& path, const std::string& bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    if (!bytes.empty()) {
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    output.close();
    return !output.fail();
}

static bool readFile(const fs::path& path, std::string& bytes)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    bytes.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    );
    return !input.bad();
}

static bool hasTemporaryOutput(const fs::path& directory)
{
    std::error_code error;
    for (fs::directory_iterator it(directory, error);
         !error && it != fs::directory_iterator();
         it.increment(error)) {
        if (it->path().filename().string().rfind(".sfv-output-", 0) == 0) {
            return true;
        }
    }
    return static_cast<bool>(error);
}

static bool check(bool condition, const char* description)
{
    std::cout << (condition ? "PASS: " : "FAIL: ")
              << description << '\n';
    return condition;
}

int main()
{
    TemporaryDirectory temporary{createTemporaryDirectory()};
    if (temporary.path.empty()) {
        std::cerr << "FAIL: could not create isolated temporary directory\n";
        return 1;
    }

    constexpr const char* expectedAbcSha256 =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

    const fs::path source = temporary.path / "abc.txt";
    const fs::path archive = temporary.path / "abc.gz";
    const fs::path restored = temporary.path / "restored.txt";
    const fs::path uppercaseRestored = temporary.path / "uppercase.txt";
    const fs::path mismatchOutput = temporary.path / "mismatch.txt";
    const fs::path malformedOutput = temporary.path / "malformed.txt";
    const fs::path existingOutput = temporary.path / "existing.txt";
    const fs::path existingCorrectOutput = temporary.path / "existing-correct.txt";

    bool passed = true;
    const std::string payload = "abc";

    passed = check(
        writeFile(source, payload),
        "source fixture is created"
    ) && passed;

    std::string calculatedDigest;
    const bool compressed = compressFile(
        source.string(),
        archive.string(),
        &calculatedDigest
    );

    passed = check(
        compressed && calculatedDigest == expectedAbcSha256,
        "compression returns the known SHA-256 digest for abc"
    ) && passed;

    std::string originalArchiveBytes;
    passed = check(
        readFile(archive, originalArchiveBytes),
        "published compressed archive can be read for no-overwrite verification"
    ) && passed;

    std::string digestAfterFailedCompression = "must-be-cleared";
    passed = check(
        !compressFile(
            source.string(),
            archive.string(),
            &digestAfterFailedCompression
        ) && digestAfterFailedCompression.empty(),
        "compression failure clears the digest output parameter"
    ) && passed;

    std::string archiveAfterFailedCompression;
    passed = check(
        readFile(archive, archiveAfterFailedCompression) &&
        archiveAfterFailedCompression == originalArchiveBytes,
        "failed compression does not overwrite an existing archive"
    ) && passed;

    passed = check(
        decompressFile(
            archive.string(),
            restored.string(),
            payload.size(),
            0600U,
            expectedAbcSha256
        ) && fs::exists(restored),
        "correct expected SHA-256 permits decompression"
    ) && passed;

    std::string restoredBytes;
    passed = check(
        readFile(restored, restoredBytes) && restoredBytes == payload,
        "correct-digest restore contains the exact original bytes"
    ) && passed;

    std::string uppercaseDigest(expectedAbcSha256);
    std::transform(
        uppercaseDigest.begin(),
        uppercaseDigest.end(),
        uppercaseDigest.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        }
    );

    passed = check(
        decompressFile(
            archive.string(),
            uppercaseRestored.string(),
            payload.size(),
            0600U,
            uppercaseDigest
        ),
        "uppercase hexadecimal expected digest is accepted"
    ) && passed;

    const std::string incorrectDigest(64, '0');
    passed = check(
        !decompressFile(
            archive.string(),
            mismatchOutput.string(),
            payload.size(),
            0600U,
            incorrectDigest
        ) && !fs::exists(mismatchOutput),
        "incorrect expected digest is rejected without publishing output"
    ) && passed;

    passed = check(
        !decompressFile(
            archive.string(),
            malformedOutput.string(),
            payload.size(),
            0600U,
            "not-a-sha256-digest"
        ) && !fs::exists(malformedOutput),
        "malformed expected digest is rejected without publishing output"
    ) && passed;

    const std::string marker = "do not overwrite this file";
    passed = check(
        writeFile(existingOutput, marker),
        "pre-existing destination fixture is created"
    ) && passed;

    passed = check(
        !decompressFile(
            archive.string(),
            existingOutput.string(),
            payload.size(),
            0600U,
            incorrectDigest
        ),
        "digest mismatch fails when destination already exists"
    ) && passed;

    std::string existingBytes;
    passed = check(
        readFile(existingOutput, existingBytes) && existingBytes == marker,
        "digest mismatch does not alter a pre-existing destination"
    ) && passed;

    passed = check(
        writeFile(existingCorrectOutput, marker),
        "second pre-existing destination fixture is created"
    ) && passed;

    passed = check(
        !decompressFile(
            archive.string(),
            existingCorrectOutput.string(),
            payload.size(),
            0600U,
            expectedAbcSha256
        ),
        "successful digest verification still cannot overwrite a destination"
    ) && passed;

    existingBytes.clear();
    passed = check(
        readFile(existingCorrectOutput, existingBytes) && existingBytes == marker,
        "correct-digest attempt preserves the pre-existing destination"
    ) && passed;

    const fs::path legacyRestored = temporary.path / "legacy.txt";
    passed = check(
        decompressFile(
            archive.string(),
            legacyRestored.string(),
            payload.size()
        ),
        "legacy decompression without an expected digest still works"
    ) && passed;

    passed = check(
        !hasTemporaryOutput(temporary.path),
        "no temporary output files remain after success and failure cases"
    ) && passed;

    if (!passed) {
        std::cerr << "SHA-256 COMPRESSION TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL SHA-256 COMPRESSION TESTS PASSED\n";
    return 0;
}

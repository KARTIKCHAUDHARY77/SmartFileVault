#include "archive.h"
#include "archive_service.h"
#include "compression.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace {
fs::path replacementSource;
fs::path replacementArchive;
bool verificationHookCalled = false;
bool replacementSucceeded = false;
}

static bool writeFile(const fs::path& path, const std::string& bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
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

static void replaceArchiveBeforeVerification(const std::string& archivePath)
{
    verificationHookCalled = true;

    std::error_code error;
    std::string ignoredDigest;
    if (!compressFile(
            replacementSource.string(),
            replacementArchive.string(),
            &ignoredDigest)) {
        return;
    }

    fs::copy_file(
        replacementArchive,
        archivePath,
        fs::copy_options::overwrite_existing,
        error
    );
    replacementSucceeded = !error;
}

static bool check(bool condition, const char* description)
{
    std::cout << (condition ? "PASS: " : "FAIL: ")
              << description << '\n';
    return condition;
}

int main()
{
    std::string pattern =
        (fs::temp_directory_path() / "sfv-archive-sha256-failure-XXXXXX").string();
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');
    char* createdDirectory = ::mkdtemp(writable.data());
    if (createdDirectory == nullptr) {
        std::cerr << "FAIL: could not create failure-test directory\n";
        return 1;
    }

    const fs::path root(createdDirectory);
    const fs::path source = root / "source.txt";
    const fs::path vault = root / "vault";
    replacementSource = root / "replacement.txt";
    replacementArchive = root / "replacement.gz";

    std::string originalPayload;
    std::string replacementPayload;
    for (int index = 0; index < 5000; ++index) {
        originalPayload += "original archive content used for SHA verification\n";
        replacementPayload += "modified archive content used for SHA verification\n";
    }

    bool passed = true;
    passed = check(
        originalPayload.size() == replacementPayload.size() &&
            writeFile(source, originalPayload) &&
            writeFile(replacementSource, replacementPayload),
        "same-size original and replacement fixtures are created"
    ) && passed;

    ArchiveMetadata metadata{};
    setArchiveServiceTestHook(&replaceArchiveBeforeVerification);
    const bool archived = archiveAndRemoveOriginal(
        source.string(),
        vault.string(),
        metadata
    );
    setArchiveServiceTestHook(nullptr);

    const fs::path metadataPath =
        vault / "metadata" / (metadata.archiveId + ".meta");

    std::string survivingOriginal;
    passed = check(
        verificationHookCalled && replacementSucceeded,
        "test hook replaces the archive with a valid same-size different-content GZIP"
    ) && passed;
    passed = check(
        !archived,
        "archiveAndRemoveOriginal rejects the digest-mismatched archive"
    ) && passed;
    passed = check(
        fs::is_regular_file(source) &&
            readFile(source, survivingOriginal) &&
            survivingOriginal == originalPayload,
        "original source remains byte-for-byte intact after digest mismatch"
    ) && passed;
    passed = check(
        !fs::exists(metadataPath) &&
            !metadata.archivePath.empty() && !fs::exists(metadata.archivePath),
        "failed archive metadata and compressed artifact are cleaned up"
    ) && passed;
    passed = check(
        !fs::exists(vault / "verify"),
        "operation-created verification directory is cleaned up"
    ) && passed;

    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    if (cleanupError) {
        std::cerr << "FAIL: test cleanup: " << cleanupError.message() << '\n';
        passed = false;
    }

    if (!passed) {
        std::cerr << "ARCHIVE SHA-256 FAILURE-PATH TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL ARCHIVE SHA-256 FAILURE-PATH TESTS PASSED\n";
    return 0;
}

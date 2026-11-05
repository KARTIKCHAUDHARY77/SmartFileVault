#include "archive.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
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
                std::cerr << "WARNING: metadata test cleanup failed: "
                          << error.message() << '\n';
            }
        }
    }
};

static fs::path makeTemporaryDirectory()
{
    std::string pattern =
        (fs::temp_directory_path() / "sfv-sha256-metadata-XXXXXX").string();
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');

    char* created = ::mkdtemp(writable.data());
    return created == nullptr ? fs::path{} : fs::path(created);
}

static bool writeMetadata(
    const fs::path& path,
    const std::vector<std::string>& optionalLines
)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    output << "SFV-METADATA-TEST\n"
           << "sample.txt\n"
           << "/original/sample.txt\n"
           << "/archive/sample.txt.gz\n"
           << "1024\n"
           << "512\n"
           << "50.0\n"
           << "1730000000\n";

    for (const std::string& line : optionalLines) {
        output << line << '\n';
    }

    output.close();
    return !output.fail();
}

static bool runLoadCase(
    const fs::path& root,
    const std::string& name,
    const std::vector<std::string>& optionalLines,
    bool expectedAccepted,
    bool expectedPermissions,
    bool expectedDigest,
    const std::string& expectedDigestValue = {}
)
{
    const fs::path path = root / (name + ".meta");
    if (!writeMetadata(path, optionalLines)) {
        std::cerr << "FAIL: could not write metadata fixture: " << name << '\n';
        return false;
    }

    ArchiveMetadata metadata{};
    const bool accepted = loadArchiveMetadata(path.string(), metadata);
    if (accepted != expectedAccepted) {
        std::cerr << "FAIL: unexpected parser result for " << name << '\n';
        return false;
    }

    if (accepted &&
        (metadata.hasOriginalPermissions != expectedPermissions ||
         metadata.hasOriginalSha256 != expectedDigest ||
         (expectedDigest && metadata.originalSha256 != expectedDigestValue))) {
        std::cerr << "FAIL: parsed optional metadata mismatch for "
                  << name << '\n';
        return false;
    }

    std::cout << "PASS: " << name << '\n';
    return true;
}

static bool check(bool condition, const char* description)
{
    std::cout << (condition ? "PASS: " : "FAIL: ")
              << description << '\n';
    return condition;
}

int main()
{
    TemporaryDirectory temporary{makeTemporaryDirectory()};
    if (temporary.path.empty()) {
        std::cerr << "FAIL: could not create metadata test directory\n";
        return 1;
    }

    const std::string digest =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    const std::string upperDigest =
        "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD";

    bool passed = true;

    // All supported historical and current optional-field layouts.
    passed = runLoadCase(
        temporary.path, "legacy-eight-lines", {}, true, false, false
    ) && passed;
    passed = runLoadCase(
        temporary.path, "legacy-permissions-only", {"420"},
        true, true, false
    ) && passed;
    passed = runLoadCase(
        temporary.path, "digest-without-permissions", {"SHA256:" + digest},
        true, false, true, digest
    ) && passed;
    passed = runLoadCase(
        temporary.path, "permissions-followed-by-digest",
        {"420", "SHA256:" + digest}, true, true, true, digest
    ) && passed;
    passed = runLoadCase(
        temporary.path, "uppercase-digest-is-valid", {"SHA256:" + upperDigest},
        true, false, true, upperDigest
    ) && passed;

    // The parser must clear digest state when a metadata object is reused.
    const fs::path validPath = temporary.path / "valid-for-reset.meta";
    const fs::path legacyPath = temporary.path / "legacy-for-reset.meta";
    ArchiveMetadata reused{};
    const bool resetFixturesReady =
        writeMetadata(validPath, {"SHA256:" + digest}) &&
        writeMetadata(legacyPath, {});
    const bool validLoaded = resetFixturesReady &&
        loadArchiveMetadata(validPath.string(), reused);
    const bool legacyLoaded = validLoaded &&
        loadArchiveMetadata(legacyPath.string(), reused);
    passed = check(
        legacyLoaded && !reused.hasOriginalSha256 && reused.originalSha256.empty(),
        "loading legacy metadata clears a previously loaded digest"
    ) && passed;

    // Malformed digest records must be rejected, not treated as legacy fields.
    passed = runLoadCase(
        temporary.path, "reject-63-character-digest",
        {"SHA256:" + std::string(63, 'a')}, false, false, false
    ) && passed;
    passed = runLoadCase(
        temporary.path, "reject-65-character-digest",
        {"SHA256:" + std::string(65, 'a')}, false, false, false
    ) && passed;

    std::string nonHexDigest = digest;
    nonHexDigest[17] = 'g';
    passed = runLoadCase(
        temporary.path, "reject-non-hexadecimal-digest",
        {"SHA256:" + nonHexDigest}, false, false, false
    ) && passed;
    passed = runLoadCase(
        temporary.path, "reject-invalid-digest-prefix",
        {"SHA-256:" + digest}, false, false, false
    ) && passed;
    passed = runLoadCase(
        temporary.path, "reject-permissions-after-digest",
        {"SHA256:" + digest, "420"}, false, false, false
    ) && passed;
    passed = runLoadCase(
        temporary.path, "reject-unexpected-third-trailing-record",
        {"420", "SHA256:" + digest, "unexpected"}, false, false, false
    ) && passed;

    // The writer must reject an invalid digest before creating a metadata file.
    ArchiveMetadata invalidToSave{};
    invalidToSave.archiveId = "invalid-digest";
    invalidToSave.originalName = "sample.txt";
    invalidToSave.originalPath = "/original/sample.txt";
    invalidToSave.archivePath = "/archive/sample.txt.gz";
    invalidToSave.originalSize = 1024;
    invalidToSave.compressedSize = 512;
    invalidToSave.compressionPercentage = 50.0;
    invalidToSave.archivedAt = 1730000000;
    invalidToSave.hasOriginalSha256 = true;
    invalidToSave.originalSha256 = std::string(63, 'a');
    const fs::path invalidSavePath = temporary.path / "invalid-save.meta";
    passed = check(
        !saveArchiveMetadata(invalidSavePath.string(), invalidToSave) &&
            !fs::exists(invalidSavePath),
        "metadata writer rejects an invalid digest without creating a file"
    ) && passed;

    if (!passed) {
        std::cerr << "ARCHIVE SHA-256 METADATA VALIDATION TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL ARCHIVE SHA-256 METADATA VALIDATION TESTS PASSED\n";
    return 0;
}

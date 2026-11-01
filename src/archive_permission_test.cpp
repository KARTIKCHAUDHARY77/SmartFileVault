#include "archive.h"
#include "compression.h"
#include "restore.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

static bool writePayload(const fs::path& path)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }

    for (int i = 0; i < 4000; ++i) {
        output << "SmartFileVault permission preservation regression.\n";
    }

    return output.good();
}

static bool hasMode(const fs::path& path, unsigned int expected)
{
    struct stat status {};
    return ::stat(path.c_str(), &status) == 0 &&
        static_cast<unsigned int>(status.st_mode & 0777) == expected;
}

static bool sameContents(const fs::path& first, const fs::path& second)
{
    std::ifstream expected(first, std::ios::binary);
    std::ifstream actual(second, std::ios::binary);

    if (!expected || !actual) {
        return false;
    }

    return std::equal(
        std::istreambuf_iterator<char>(expected),
        std::istreambuf_iterator<char>(),
        std::istreambuf_iterator<char>(actual),
        std::istreambuf_iterator<char>()
    );
}

static bool restoreNewMetadataCase(
    const fs::path& root,
    const std::string& name,
    unsigned int sourceMode,
    fs::path& archivePath,
    std::uintmax_t& originalSize)
{
    const fs::path source = root / (name + ".txt");
    const fs::path archiveRoot = root / (name + "-vault");
    const fs::path restoreRoot = root / (name + "-restore");

    if (!writePayload(source) ||
        ::chmod(source.c_str(), static_cast<mode_t>(sourceMode)) != 0 ||
        !fs::create_directories(restoreRoot)) {
        return false;
    }

    ArchiveMetadata created {};
    if (!createArchive(source.string(), archiveRoot.string(), created)) {
        return false;
    }

    if (!created.hasOriginalPermissions ||
        created.originalPermissions != sourceMode) {
        std::cerr << "FAIL: source permissions were not recorded for "
                  << name << '\n';
        return false;
    }

    ArchiveMetadata loaded {};
    const fs::path metadataPath =
        archiveRoot / "metadata" / (created.archiveId + ".meta");

    if (!loadArchiveMetadata(metadataPath.string(), loaded) ||
        !loaded.hasOriginalPermissions ||
        loaded.originalPermissions != sourceMode) {
        std::cerr << "FAIL: permission metadata did not round-trip for "
                  << name << '\n';
        return false;
    }

    archivePath = created.archivePath;
    originalSize = created.originalSize;

    if (!hasMode(archivePath, 0600U)) {
        std::cerr << "FAIL: compressed archive is not private (0600)\n";
        return false;
    }

    std::string restored;
    if (!restoreArchive(
            metadataPath.string(),
            restoreRoot.string(),
            restored)) {
        std::cerr << "FAIL: restore failed for " << name << '\n';
        return false;
    }

    if (!hasMode(restored, sourceMode) ||
        !sameContents(source, restored)) {
        std::cerr << "FAIL: restored mode or bytes mismatch for "
                  << name << '\n';
        return false;
    }

    std::cout << "PASS: new metadata restores mode "
              << sourceMode << " for " << name << '\n';
    return true;
}

static bool restoreLegacyMetadataCase(
    const fs::path& root,
    const fs::path& archivePath,
    std::uintmax_t originalSize,
    const fs::path& reference)
{
    const fs::path metadataPath = root / "legacy-eight-line.meta";
    const fs::path restoreRoot = root / "legacy-restore";

    if (!fs::create_directories(restoreRoot)) {
        return false;
    }

    ArchiveMetadata legacy {};
    legacy.archiveId = "legacy-eight-line";
    legacy.originalName = "legacy-restored.txt";
    legacy.originalPath =
        (root / "missing-original" / "legacy-restored.txt").string();
    legacy.archivePath = archivePath.string();
    legacy.originalSize = originalSize;
    legacy.compressedSize = fs::file_size(archivePath);
    legacy.compressionPercentage = 99.0;
    legacy.archivedAt = 1;
    legacy.hasOriginalPermissions = false;

    if (!saveArchiveMetadata(metadataPath.string(), legacy)) {
        return false;
    }

    ArchiveMetadata loaded {};
    if (!loadArchiveMetadata(metadataPath.string(), loaded) ||
        loaded.hasOriginalPermissions ||
        loaded.originalPermissions != 0600U) {
        std::cerr << "FAIL: legacy eight-line metadata fallback\n";
        return false;
    }

    std::string restored;
    if (!restoreArchive(
            metadataPath.string(),
            restoreRoot.string(),
            restored)) {
        return false;
    }

    if (!hasMode(restored, 0600U) ||
        fs::file_size(restored) != originalSize ||
        !sameContents(reference, restored)) {
        std::cerr << "FAIL: legacy metadata restore did not use 0600\n";
        return false;
    }

    std::cout << "PASS: legacy eight-line metadata restores as 0600\n";
    return true;
}

int main()
{
    char pattern[] = "/tmp/sfv-archive-permissions.XXXXXX";
    char* directory = ::mkdtemp(pattern);
    if (directory == nullptr) {
        std::cerr << "FAIL: could not create isolated test directory\n";
        return 1;
    }

    const fs::path root(directory);
    bool passed = true;
    fs::path archivePath;
    std::uintmax_t originalSize = 0;
    fs::path privateArchive;
    std::uintmax_t privateOriginalSize = 0;

    const bool publicModePassed = restoreNewMetadataCase(
        root, "mode-0644", 0644U, archivePath, originalSize
    );
    passed = publicModePassed && passed;

    const bool privateModePassed = restoreNewMetadataCase(
        root, "mode-0600", 0600U, privateArchive, privateOriginalSize
    );
    passed = privateModePassed && passed;

    if (publicModePassed) {
        passed = restoreLegacyMetadataCase(
            root,
            archivePath,
            originalSize,
            root / "mode-0644.txt"
        ) && passed;
    }

    if (passed) {
        std::error_code cleanupError;
        fs::remove_all(root, cleanupError);
        if (cleanupError) {
            std::cerr << "FAIL: test directory cleanup: "
                      << cleanupError.message() << '\n';
            passed = false;
        }
    }

    if (!passed) {
        std::cerr
            << "ARCHIVE PERMISSION TESTS FAILED; preserving test directory: "
            << root.string() << '\n';
        return 1;
    }

    std::cout << "ALL ARCHIVE PERMISSION TESTS PASSED\n";
    return 0;
}

#include "archive.h"
#include "archive_service.h"
#include "compression.h"
#include "restore.h"

#include <algorithm>
#include <cstdint>
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
                std::cerr << "WARNING: test cleanup failed: "
                          << error.message() << '\n';
            }
        }
    }
};

static fs::path makeTemporaryDirectory()
{
    std::error_code error;
    const fs::path tempRoot = fs::temp_directory_path(error);

    if (error) {
        std::cerr << "FAIL: could not locate temporary directory: "
                  << error.message() << '\n';
        return {};
    }

    std::string pattern =
        (tempRoot / "sfv_archive_sha256.XXXXXX").string();

    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');

    char* created = ::mkdtemp(writable.data());

    return created == nullptr ? fs::path{} : fs::path(created);
}

static bool writeFile(
    const fs::path& path,
    const std::string& bytes
)
{
    std::ofstream output(
        path,
        std::ios::binary | std::ios::trunc
    );

    if (!output) {
        return false;
    }

    if (!bytes.empty()) {
        output.write(
            bytes.data(),
            static_cast<std::streamsize>(bytes.size())
        );
    }

    output.close();

    return !output.fail();
}

static bool readFile(
    const fs::path& path,
    std::string& bytes
)
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

static bool isLowerHexDigest(const std::string& digest)
{
    if (digest.size() != 64) {
        return false;
    }

    return std::all_of(
        digest.begin(),
        digest.end(),
        [](unsigned char character) {
            return
                (character >= '0' && character <= '9') ||
                (character >= 'a' && character <= 'f');
        }
    );
}

static bool hasTemporaryOutput(const fs::path& directory)
{
    std::error_code error;

    fs::directory_iterator it(directory, error);
    const fs::directory_iterator end;

    for (; !error && it != end; it.increment(error)) {
        if (it->path().filename().string().rfind(
                ".sfv-output-", 0
            ) == 0) {
            return true;
        }
    }

    return static_cast<bool>(error);
}

static bool check(
    bool passed,
    const char* message
)
{
    std::cout
        << (passed ? "PASS: " : "FAIL: ")
        << message << '\n';

    return passed;
}

int main()
{
    TemporaryDirectory temporary{
        makeTemporaryDirectory()
    };

    if (temporary.path.empty()) {
        std::cerr
            << "FAIL: could not create temporary test directory\n";
        return 1;
    }

    const std::string line =
        "SmartFileVault archive SHA-256 metadata integration regression.\n";

    std::string payload;

    payload.reserve(line.size() * 6000U);

    for (int i = 0; i < 6000; ++i) {
        payload += line;
    }

    const fs::path source =
        temporary.path / "original.txt";

    const fs::path vault =
        temporary.path / "vault";

    const fs::path restoreDirectory =
        temporary.path / "restore-good";

    if (!check(
            writeFile(source, payload),
            "source fixture is created")) {
        return 1;
    }

    // --------------------------------------------------------
    // TEST 1: Create archive and record SHA-256 metadata
    // --------------------------------------------------------

    ArchiveMetadata created{};

    const bool createSucceeded =
        createArchive(
            source.string(),
            vault.string(),
            created
        );

    if (!check(
            createSucceeded,
            "createArchive creates a compressed archive")) {
        return 1;
    }

    if (!check(
            created.hasOriginalSha256 &&
            isLowerHexDigest(created.originalSha256),
            "createArchive stores a 64-character lowercase SHA-256 digest")) {
        return 1;
    }

    // --------------------------------------------------------
    // TEST 2: SHA-256 metadata round-trip
    // --------------------------------------------------------

    const fs::path metadataPath =
        vault /
        "metadata" /
        (created.archiveId + ".meta");

    ArchiveMetadata loaded{};

    const bool metadataLoaded =
        loadArchiveMetadata(
            metadataPath.string(),
            loaded
        );

    if (!check(
            metadataLoaded &&
            loaded.hasOriginalSha256 &&
            loaded.originalSha256 == created.originalSha256,
            "SHA-256 digest round-trips through archive metadata")) {
        return 1;
    }

    if (!check(
            loaded.hasOriginalPermissions,
            "existing permission metadata remains present alongside SHA-256")) {
        return 1;
    }

    // --------------------------------------------------------
    // TEST 3: Restore with correct SHA-256
    // --------------------------------------------------------

    std::string restoredPath;

    const bool restored =
        restoreArchive(
            metadataPath.string(),
            restoreDirectory.string(),
            restoredPath
        );

    std::string restoredBytes;

    if (!check(
            restored &&
            readFile(restoredPath, restoredBytes) &&
            restoredBytes == payload,
            "restoreArchive verifies the digest and restores exact original bytes")) {
        return 1;
    }

    // --------------------------------------------------------
    // TEST 4: Tampered archive with same decompressed size
    // --------------------------------------------------------

    // Replace the archive with a different, valid GZIP stream
    // that expands to the same number of bytes.
    //
    // This isolates SHA-256 verification from GZIP checksum
    // and expected-size validation.

    std::string differentPayload = payload;

    for (char& character : differentPayload) {
        if (character == 'S') {
            character = 'X';
        }
    }

    const fs::path differentSource =
        temporary.path / "different.txt";

    const fs::path differentGzip =
        temporary.path / "different.gz";

    std::string ignoredDigest;

    const bool differentFixtureReady =
        writeFile(
            differentSource,
            differentPayload
        ) &&
        differentPayload.size() == payload.size() &&
        compressFile(
            differentSource.string(),
            differentGzip.string(),
            &ignoredDigest
        );

    std::error_code copyError;

    bool tamperedArchiveInstalled = false;

    if (differentFixtureReady) {
        tamperedArchiveInstalled =
            fs::copy_file(
                differentGzip,
                created.archivePath,
                fs::copy_options::overwrite_existing,
                copyError
            ) && !copyError;
    }

    if (!check(
            differentFixtureReady && tamperedArchiveInstalled,
            "same-size but different valid GZIP fixture replaces archive for tamper test")) {
        return 1;
    }

    // --------------------------------------------------------
    // TEST 5: Tampered archive must not be restored
    // --------------------------------------------------------

    const fs::path tamperedRestoreDirectory =
        temporary.path / "restore-tampered";

    std::error_code directoryError;

    fs::create_directories(
        tamperedRestoreDirectory,
        directoryError
    );

    if (!check(
            !directoryError &&
            fs::is_directory(tamperedRestoreDirectory),
            "tampered restore directory is created")) {
        return 1;
    }

    std::string tamperedRestoredPath;

    const bool tamperedRestoreSucceeded =
        restoreArchive(
            metadataPath.string(),
            tamperedRestoreDirectory.string(),
            tamperedRestoredPath
        );

    const fs::path expectedTamperedOutput =
        tamperedRestoreDirectory / created.originalName;

    if (!check(
            !tamperedRestoreSucceeded &&
            tamperedRestoredPath.empty() &&
            !fs::exists(expectedTamperedOutput),
            "restore rejects a valid same-size archive with the wrong digest before publication")) {
        return 1;
    }

    if (!check(
            !hasTemporaryOutput(tamperedRestoreDirectory),
            "digest rejection leaves no temporary restore output")) {
        return 1;
    }

    // --------------------------------------------------------
    // TEST 6: Legacy metadata without SHA-256
    // --------------------------------------------------------

    // Legacy metadata intentionally omits permissions and
    // SHA-256. The old restore path must remain functional.

    const fs::path legacyArchive =
        temporary.path / "legacy.gz";

    const fs::path legacyMetadataPath =
        temporary.path / "legacy.meta";

    const fs::path legacyRestoreDirectory =
        temporary.path / "restore-legacy";

    std::string legacyDigest;

    const bool legacyArchiveCreated =
        compressFile(
            source.string(),
            legacyArchive.string(),
            &legacyDigest
        );

    // Important: never call file_size() unless archive
    // creation has succeeded.

    if (!check(
            legacyArchiveCreated,
            "legacy GZIP fixture is created")) {
        return 1;
    }

    std::error_code legacySizeError;

    const std::uintmax_t legacyCompressedSize =
        fs::file_size(
            legacyArchive,
            legacySizeError
        );

    if (!check(
            !legacySizeError,
            "legacy GZIP fixture size is readable")) {
        return 1;
    }

    ArchiveMetadata legacy{};

    legacy.archiveId = "legacy-archive";

    legacy.originalName = "legacy-original.txt";

    legacy.originalPath =
        (
            temporary.path /
            "missing-original" /
            legacy.originalName
        ).string();

    legacy.archivePath =
        fs::absolute(legacyArchive).string();

    legacy.originalSize =
        static_cast<std::uintmax_t>(payload.size());

    legacy.compressedSize =
        legacyCompressedSize;

    legacy.compressionPercentage = 90.0;

    legacy.archivedAt = 1;

    legacy.hasOriginalPermissions = false;

    legacy.hasOriginalSha256 = false;

    const bool legacyMetadataSaved =
        saveArchiveMetadata(
            legacyMetadataPath.string(),
            legacy
        );

    if (!check(
            legacyMetadataSaved,
            "legacy metadata without permissions or SHA-256 is saved")) {
        return 1;
    }

    std::string legacyRestoredPath;

    const bool legacyRestored =
        restoreArchive(
            legacyMetadataPath.string(),
            legacyRestoreDirectory.string(),
            legacyRestoredPath
        );

    std::string legacyRestoredBytes;

    if (!check(
            legacyRestored &&
            readFile(
                legacyRestoredPath,
                legacyRestoredBytes
            ) &&
            legacyRestoredBytes == payload,
            "legacy metadata without SHA-256 still restores successfully")) {
        return 1;
    }

    // --------------------------------------------------------
    // TEST 7: Archive-and-remove service integration
    // --------------------------------------------------------

    // The archive-and-remove service must verify the stored
    // digest before it is permitted to delete the original.

    const fs::path serviceSource =
        temporary.path / "service-source.txt";

    const fs::path serviceVault =
        temporary.path / "service-vault";

    if (!check(
            writeFile(serviceSource, payload),
            "archive-service source fixture is created")) {
        return 1;
    }

    ArchiveMetadata serviceMetadata{};

    const bool serviceArchived =
        archiveAndRemoveOriginal(
            serviceSource.string(),
            serviceVault.string(),
            serviceMetadata
        );

    if (!check(
            serviceArchived &&
            !fs::exists(serviceSource) &&
            serviceMetadata.hasOriginalSha256 &&
            isLowerHexDigest(serviceMetadata.originalSha256),
            "archiveAndRemoveOriginal retains SHA-256 metadata after verified archiving")) {
        return 1;
    }

    // --------------------------------------------------------
    // FINAL RESULT
    // --------------------------------------------------------

    std::cout
        << "ALL ARCHIVE SHA-256 INTEGRATION TESTS PASSED\n";

    return 0;
}
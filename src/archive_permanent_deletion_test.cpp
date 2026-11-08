#include "archive_service.h"
#include "compression.h"

#include <chrono>
#include <cstdlib>
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
    const fs::path base = fs::temp_directory_path(error);
    if (error) {
        return {};
    }

    std::string pattern = (base / "sfv_archive_delete.XXXXXX").string();
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');
    char* created = ::mkdtemp(writable.data());
    return created == nullptr ? fs::path{} : fs::path(created);
}

static bool writePayload(const fs::path& path, const std::string& payload)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    for (int index = 0; index < 1200; ++index) {
        file.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    }
    file.close();
    return !file.fail();
}

static bool readText(const fs::path& path, std::string& contents)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    contents.assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
    return !file.bad();
}

static bool check(bool passed, const char* description)
{
    std::cout << (passed ? "PASS: " : "FAIL: ")
              << description << '\n';
    return passed;
}

static bool createFixture(
    const fs::path& root,
    const fs::path& original,
    ArchiveMetadata& metadata
)
{
    if (!writePayload(
            original,
            "SmartFileVault permanent deletion fixture line.\n")) {
        return false;
    }
    return createArchive(original.string(), root.string(), metadata);
}

static fs::path metadataPath(const fs::path& root, const ArchiveMetadata& metadata)
{
    return root / "metadata" / (metadata.archiveId + ".meta");
}

static bool testDeleteVerifiedOriginal(const fs::path& root)
{
    const fs::path original = root / "original" / "verified.txt";
    const fs::path vault = root / "vault-verified";
    ArchiveMetadata metadata{};

    if (!fs::create_directories(original.parent_path()) ||
        !createFixture(vault, original, metadata)) {
        return check(false, "verified-original fixture created");
    }

    const bool identityStored = metadata.hasOriginalFileIdentity &&
        metadata.originalFileIdentity.inode != 0 &&
        metadata.originalFileIdentity.size == metadata.originalSize;
    if (!check(identityStored, "new archive metadata stores original filesystem identity")) {
        return false;
    }

    ArchiveMetadata loaded{};
    if (!check(
            loadArchiveMetadata(metadataPath(vault, metadata).string(), loaded) &&
            loaded.hasOriginalFileIdentity &&
            loaded.originalFileIdentity.device == metadata.originalFileIdentity.device &&
            loaded.originalFileIdentity.inode == metadata.originalFileIdentity.inode &&
            loaded.originalFileIdentity.modifiedNanoseconds == metadata.originalFileIdentity.modifiedNanoseconds,
            "versioned filesystem identity round-trips through archive metadata")) {
        return false;
    }

    ArchiveDeletionResult result;
    const bool deleted = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, result
    );

    const bool passed = deleted && result.archiveDeleted &&
        result.compressedRemoved && result.metadataRemoved &&
        result.originalDeleted && !fs::exists(original) &&
        !fs::exists(metadata.archivePath) &&
        !fs::exists(metadataPath(vault, metadata));

    if (!check(passed, "matching original, compressed archive, and metadata are deleted")) {
        return false;
    }

    ArchiveDeletionResult repeated;
    const bool repeatedDelete = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, repeated
    );
    return check(
        !repeatedDelete && repeated.errorCode == "archive_not_found",
        "repeated deletion returns archive_not_found without touching unrelated files"
    );
}

static bool testSameContentReplacementPreserved(const fs::path& root)
{
    const fs::path original = root / "replacement" / "document.txt";
    const fs::path movedOriginal = root / "replacement" / "original-object.txt";
    const fs::path replacement = root / "replacement" / "new-object.txt";
    const fs::path vault = root / "vault-replacement";
    ArchiveMetadata metadata{};

    fs::create_directories(original.parent_path());
    if (!createFixture(vault, original, metadata) ||
        !writePayload(replacement, "SmartFileVault permanent deletion fixture line.\n")) {
        return check(false, "replacement fixture created");
    }

    std::error_code error;
    fs::rename(original, movedOriginal, error);
    if (error) {
        return check(false, "original file moved aside for replacement test");
    }
    fs::rename(replacement, original, error);
    if (error) {
        return check(false, "replacement file installed at original path");
    }

    std::string replacementBytes;
    ArchiveDeletionResult result;
    const bool deleted = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, result
    );

    const bool passed = deleted && result.archiveDeleted &&
        result.originalPreserved &&
        result.originalStatus == "preserved_identity_mismatch" &&
        fs::exists(original) &&
        readText(original, replacementBytes) &&
        replacementBytes.size() == metadata.originalSize &&
        !fs::exists(metadata.archivePath);

    return check(
        passed,
        "same-content replacement at original path is preserved by device/inode identity check"
    );
}

static bool testLegacyOriginalPreserved(const fs::path& root)
{
    const fs::path original = root / "legacy" / "legacy.txt";
    const fs::path vault = root / "vault-legacy";
    ArchiveMetadata metadata{};
    fs::create_directories(original.parent_path());

    if (!createFixture(vault, original, metadata)) {
        return check(false, "legacy archive fixture created");
    }

    metadata.hasOriginalFileIdentity = false;
    metadata.originalFileIdentity = OriginalFileIdentity{};
    if (!saveArchiveMetadata(metadataPath(vault, metadata).string(), metadata)) {
        return check(false, "legacy metadata without identity saved");
    }

    ArchiveDeletionResult result;
    const bool deleted = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, result
    );

    return check(
        deleted && result.archiveDeleted && result.originalPreserved &&
        result.originalStatus == "preserved_legacy_no_identity" &&
        fs::exists(original),
        "legacy archive without identity is deleted while its original is preserved"
    );
}

static bool testMissingOriginalAndParent(const fs::path& root)
{
    const fs::path original = root / "missing" / "missing.txt";
    const fs::path vault = root / "vault-missing";
    ArchiveMetadata metadata{};
    fs::create_directories(original.parent_path());

    if (!createFixture(vault, original, metadata)) {
        return check(false, "missing-original fixture created");
    }
    fs::remove(original);
    fs::remove(original.parent_path());

    ArchiveDeletionResult result;
    const bool deleted = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, result
    );

    return check(
        deleted && result.archiveDeleted && result.originalMissing &&
        result.originalStatus == "already_missing",
        "missing original and missing parent directory do not block archive deletion"
    );
}

static bool testMissingCompressedArchivePreservesOriginal(const fs::path& root)
{
    const fs::path original = root / "missing-gzip" / "keep.txt";
    const fs::path vault = root / "vault-missing-gzip";
    ArchiveMetadata metadata{};
    fs::create_directories(original.parent_path());

    if (!createFixture(vault, original, metadata)) {
        return check(false, "missing-compressed fixture created");
    }
    fs::remove(metadata.archivePath);

    ArchiveDeletionResult result;
    const bool deleted = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, result
    );

    return check(
        deleted && result.archiveDeleted && result.originalPreserved &&
        result.originalStatus == "preserved_archive_missing" &&
        fs::exists(original),
        "missing compressed archive causes original preservation while stale metadata is removed"
    );
}

static bool testInvalidIdAndMissingMetadata(const fs::path& root)
{
    const fs::path outside = root / "outside.txt";
    if (!writePayload(outside, "sentinel\n")) {
        return check(false, "outside sentinel created");
    }

    ArchiveDeletionResult invalid;
    const bool invalidDelete = permanentlyDeleteArchive(
        (root / "vault-invalid").string(), "../outside", invalid
    );

    std::string sentinel;
    const bool sentinelPreserved = readText(outside, sentinel);
    if (!check(
            !invalidDelete && invalid.errorCode == "invalid_archive_id" &&
            sentinelPreserved,
            "invalid archive ID and traversal are rejected before path construction")) {
        return false;
    }

    const fs::path original = root / "orphan" / "orphan.txt";
    const fs::path vault = root / "vault-orphan";
    ArchiveMetadata metadata{};
    fs::create_directories(original.parent_path());

    if (!createFixture(vault, original, metadata)) {
        return check(false, "orphan archive fixture created");
    }
    fs::remove(metadataPath(vault, metadata));

    ArchiveDeletionResult missing;
    const bool missingDelete = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, missing
    );

    return check(
        !missingDelete && missing.errorCode == "archive_not_found" &&
        fs::exists(metadata.archivePath) && fs::exists(original),
        "missing metadata does not cause deletion of an unverified compressed artifact or original"
    );
}

static bool testMalformedMetadataPreserved(const fs::path& root)
{
    const fs::path original = root / "malformed" / "malformed.txt";
    const fs::path vault = root / "vault-malformed";
    ArchiveMetadata metadata{};
    fs::create_directories(original.parent_path());

    if (!createFixture(vault, original, metadata)) {
        return check(false, "malformed-metadata fixture created");
    }
    {
        std::ofstream file(metadataPath(vault, metadata), std::ios::app);
        file << "UNEXPECTED-TRAILING-RECORD\n";
    }

    ArchiveDeletionResult result;
    const bool deleted = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, result
    );

    return check(
        !deleted && result.errorCode == "invalid_metadata" &&
        fs::exists(original) && fs::exists(metadata.archivePath) &&
        fs::exists(metadataPath(vault, metadata)),
        "malformed metadata is rejected without deleting any related files"
    );
}

static bool testMalformedFileIdentityPreserved(const fs::path& root)
{
    const fs::path original = root / "bad-fileid" / "bad-fileid.txt";
    const fs::path vault = root / "vault-bad-fileid";
    ArchiveMetadata metadata{};
    fs::create_directories(original.parent_path());

    if (!createFixture(vault, original, metadata)) {
        return check(false, "malformed FILEID fixture created");
    }

    const fs::path metaPath = metadataPath(vault, metadata);
    std::string metadataBytes;
    if (!readText(metaPath, metadataBytes)) {
        return check(false, "metadata read for malformed FILEID test");
    }

    const std::string prefix = "FILEID:v1:";
    const std::size_t start = metadataBytes.find(prefix);
    if (start == std::string::npos) {
        return check(false, "FILEID record found in new metadata");
    }
    const std::size_t end = metadataBytes.find('\n', start);
    if (end == std::string::npos) {
        return check(false, "FILEID record has a line terminator");
    }

    const std::string invalidRecord =
        "FILEID:v1:1:2:" + std::to_string(metadata.originalSize) +
        ":0:1000000000:0:0:0:0";
    metadataBytes.replace(start, end - start, invalidRecord);
    {
        std::ofstream output(metaPath, std::ios::binary | std::ios::trunc);
        output << metadataBytes;
        if (!output) {
            return check(false, "malformed FILEID metadata written");
        }
    }

    ArchiveDeletionResult result;
    const bool deleted = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, result
    );

    return check(
        !deleted && result.errorCode == "invalid_metadata" &&
        fs::exists(original) && fs::exists(metadata.archivePath) &&
        fs::exists(metaPath),
        "malformed filesystem identity record is rejected without deleting files"
    );
}


static bool testUnsafeRecordedArchivePathPreserved(const fs::path& root)
{
    const fs::path original = root / "unsafe-path" / "unsafe.txt";
    const fs::path vault = root / "vault-unsafe-path";
    const fs::path sentinel = root / "external-archive-sentinel.gz";
    ArchiveMetadata metadata{};
    fs::create_directories(original.parent_path());

    if (!createFixture(vault, original, metadata) ||
        !writePayload(sentinel, "external sentinel\n")) {
        return check(false, "unsafe-path fixture created");
    }

    metadata.archivePath = fs::absolute(sentinel).string();
    if (!saveArchiveMetadata(metadataPath(vault, metadata).string(), metadata)) {
        return check(false, "tampered archive path metadata saved");
    }

    ArchiveDeletionResult result;
    const bool deleted = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, result
    );

    return check(
        !deleted && result.errorCode == "unsafe_archive_path" &&
        fs::exists(sentinel) && fs::exists(original) &&
        fs::exists(metadataPath(vault, metadata)),
        "metadata path pointing outside the vault is rejected without touching external files"
    );
}

#ifdef SFV_ENABLE_TEST_HOOK
static std::string deletionFailurePoint;

static bool failSelectedDeletionStep(const std::string& step)
{
    return step == deletionFailurePoint;
}

struct DeletionHookReset {
    ~DeletionHookReset() {
        setArchiveDeletionTestHook(nullptr);
        deletionFailurePoint.clear();
    }
};

static bool testMetadataRemovalPartialFailure(const fs::path& root)
{
    const fs::path original = root / "partial-metadata" / "source.txt";
    const fs::path vault = root / "vault-partial-metadata";
    ArchiveMetadata metadata{};
    fs::create_directories(original.parent_path());

    if (!createFixture(vault, original, metadata)) {
        return check(false, "metadata-partial-failure fixture created");
    }

    DeletionHookReset reset;
    deletionFailurePoint = "metadata_unlink";
    setArchiveDeletionTestHook(failSelectedDeletionStep);

    ArchiveDeletionResult result;
    const bool deleted = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, result
    );

    return check(
        !deleted && result.partialFailure && !result.archiveDeleted &&
        result.compressedRemoved && !result.metadataRemoved &&
        result.originalPreserved && result.errorCode == "metadata_delete_failed" &&
        !fs::exists(metadata.archivePath) &&
        fs::exists(metadataPath(vault, metadata)) && fs::exists(original),
        "metadata unlink failure reports partial state and preserves metadata plus original"
    );
}

static bool testOriginalRemovalPartialFailure(const fs::path& root)
{
    const fs::path original = root / "partial-original" / "source.txt";
    const fs::path vault = root / "vault-partial-original";
    ArchiveMetadata metadata{};
    fs::create_directories(original.parent_path());

    if (!createFixture(vault, original, metadata)) {
        return check(false, "original-partial-failure fixture created");
    }

    DeletionHookReset reset;
    deletionFailurePoint = "original_unlink";
    setArchiveDeletionTestHook(failSelectedDeletionStep);

    ArchiveDeletionResult result;
    const bool deleted = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, result
    );

    return check(
        !deleted && result.partialFailure && result.archiveDeleted &&
        result.compressedRemoved && result.metadataRemoved &&
        result.originalPreserved && !result.originalDeleted &&
        result.errorCode == "original_delete_failed" &&
        !fs::exists(metadata.archivePath) &&
        !fs::exists(metadataPath(vault, metadata)) && fs::exists(original),
        "original unlink failure reports archive removal and preserves the original for manual recovery"
    );
}
#endif

static bool testOriginalSymlinkPreserved(const fs::path& root)
{
    const fs::path original = root / "symlink" / "link.txt";
    const fs::path target = root / "symlink" / "target.txt";
    const fs::path vault = root / "vault-symlink";
    ArchiveMetadata metadata{};
    fs::create_directories(original.parent_path());

    if (!createFixture(vault, original, metadata) ||
        !writePayload(target, "target sentinel\n")) {
        return check(false, "symlink fixture created");
    }
    fs::remove(original);
    std::error_code error;
    fs::create_symlink(target, original, error);
    if (error) {
        return check(false, "replacement symlink installed");
    }

    ArchiveDeletionResult result;
    const bool deleted = permanentlyDeleteArchive(
        vault.string(), metadata.archiveId, result
    );
    std::string targetContents;

    return check(
        deleted && result.archiveDeleted && result.originalPreserved &&
        result.originalStatus == "preserved_symlink" &&
        fs::is_symlink(original) && readText(target, targetContents),
        "symlink at original path is preserved and its target is never followed or deleted"
    );
}

int main()
{
    TemporaryDirectory temporary{makeTemporaryDirectory()};
    if (temporary.path.empty()) {
        std::cerr << "FAIL: could not create temporary directory\n";
        return 1;
    }

    bool passed = true;
    passed = testDeleteVerifiedOriginal(temporary.path) && passed;
    passed = testSameContentReplacementPreserved(temporary.path) && passed;
    passed = testLegacyOriginalPreserved(temporary.path) && passed;
    passed = testMissingOriginalAndParent(temporary.path) && passed;
    passed = testMissingCompressedArchivePreservesOriginal(temporary.path) && passed;
    passed = testInvalidIdAndMissingMetadata(temporary.path) && passed;
    passed = testMalformedMetadataPreserved(temporary.path) && passed;
    passed = testMalformedFileIdentityPreserved(temporary.path) && passed;
    passed = testUnsafeRecordedArchivePathPreserved(temporary.path) && passed;
    passed = testOriginalSymlinkPreserved(temporary.path) && passed;
#ifdef SFV_ENABLE_TEST_HOOK
    passed = testMetadataRemovalPartialFailure(temporary.path) && passed;
    passed = testOriginalRemovalPartialFailure(temporary.path) && passed;
#endif

    if (!passed) {
        std::cerr << "ARCHIVE PERMANENT DELETION TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL ARCHIVE PERMANENT DELETION TESTS PASSED\n";
    return 0;
}

#include "archive_service.h"
#include "vault_service.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::string failureStep;

#ifdef SFV_ENABLE_TEST_HOOK
bool failCleanupStep(const std::string& step)
{
    return step == failureStep;
}
#endif

struct TemporaryDirectory {
    fs::path path;
    ~TemporaryDirectory()
    {
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

fs::path makeTemp()
{
    std::error_code error;
    const fs::path base = fs::temp_directory_path(error);
    if (error) return {};

    std::string pattern = (base / "sfv_restore_cleanup.XXXXXX").string();
    std::vector<char> value(pattern.begin(), pattern.end());
    value.push_back('\0');
    char* created = ::mkdtemp(value.data());
    return created ? fs::path(created) : fs::path{};
}

bool check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << label << '\n';
    return condition;
}

bool writeFixture(const fs::path& path, std::string& bytes)
{
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    if (error) return false;

    bytes.clear();
    for (int i = 0; i < 2000; ++i) {
        bytes += "Restore cleanup byte-exact test fixture.\n";
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    return !out.fail();
}

bool readBytes(const fs::path& path, std::string& bytes)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !in.bad();
}

bool makeArchive(
    const fs::path& root,
    const std::string& tag,
    fs::path& vault,
    ArchiveMetadata& metadata,
    std::string& expected
)
{
    const fs::path source = root / (tag + ".bin");
    vault = root / (tag + "-vault");
    return writeFixture(source, expected) &&
           archiveAndRemoveOriginal(source.string(), vault.string(), metadata);
}
}

int main()
{
#ifdef SFV_ENABLE_TEST_HOOK
    setArchiveDeletionTestHook(failCleanupStep);
#endif

    TemporaryDirectory temporary{makeTemp()};
    if (!check(!temporary.path.empty(), "temporary fixture directory created")) {
        return 1;
    }

    // Successful restore: verify the output and confirm both artifacts disappear.
    fs::path vault;
    ArchiveMetadata metadata{};
    std::string expected;
    if (!check(makeArchive(temporary.path, "success", vault, metadata, expected),
               "success archive created from disposable fixture")) {
        return 1;
    }

    ArchiveRestoreResult success{};
    const bool successReturned = restoreFromArchive(
        vault.string(), metadata.archiveId,
        (temporary.path / "success-out").string(), success
    );
    if (!check(successReturned && success.restored && success.archiveDeleted &&
               success.compressedRemoved && success.metadataRemoved &&
               !success.partialFailure,
               "successful restore removes both archive artifacts")) {
        return 1;
    }

    std::string actual;
    if (!check(readBytes(success.restoredPath, actual) && actual == expected,
               "restored output is byte-for-byte identical")) {
        return 1;
    }
    if (!check(getAllArchives(vault.string()).empty(),
               "restored archive is removed from catalogue")) {
        return 1;
    }

#ifdef SFV_ENABLE_TEST_HOOK
    // Failure to unlink the compressed artifact must preserve both vault artifacts.
    fs::path compressedFailureVault;
    ArchiveMetadata compressedFailureMeta{};
    std::string compressedFailureBytes;
    if (!check(makeArchive(temporary.path, "compressed-failure",
                           compressedFailureVault, compressedFailureMeta,
                           compressedFailureBytes),
               "compressed failure fixture archived")) {
        return 1;
    }

    failureStep = "restore_compressed_unlink";
    ArchiveRestoreResult compressedFailure{};
    if (!check(restoreFromArchive(
                   compressedFailureVault.string(), compressedFailureMeta.archiveId,
                   (temporary.path / "compressed-failure-out").string(), compressedFailure) &&
               compressedFailure.restored && compressedFailure.partialFailure &&
               !compressedFailure.compressedRemoved &&
               fs::exists(compressedFailureMeta.archivePath) &&
               fs::exists(compressedFailureVault / "metadata" /
                          (compressedFailureMeta.archiveId + ".meta")) &&
               fs::exists(compressedFailure.restoredPath),
               "compressed deletion failure preserves vault artifacts and restored output")) {
        return 1;
    }

    // Failure to unlink metadata leaves the metadata catalogue entry discoverable.
    fs::path metadataFailureVault;
    ArchiveMetadata metadataFailureMeta{};
    std::string metadataFailureBytes;
    if (!check(makeArchive(temporary.path, "metadata-failure",
                           metadataFailureVault, metadataFailureMeta,
                           metadataFailureBytes),
               "metadata failure fixture archived")) {
        return 1;
    }

    failureStep = "restore_metadata_unlink";
    ArchiveRestoreResult metadataFailure{};
    const fs::path remainingMetadata = metadataFailureVault / "metadata" /
        (metadataFailureMeta.archiveId + ".meta");
    if (!check(restoreFromArchive(
                   metadataFailureVault.string(), metadataFailureMeta.archiveId,
                   (temporary.path / "metadata-failure-out").string(), metadataFailure) &&
               metadataFailure.restored && metadataFailure.partialFailure &&
               metadataFailure.compressedRemoved && !metadataFailure.metadataRemoved &&
               !fs::exists(metadataFailureMeta.archivePath) &&
               fs::exists(remainingMetadata) &&
               getAllArchives(metadataFailureVault.string()).size() == 1 &&
               fs::exists(metadataFailure.restoredPath),
               "metadata deletion failure reports partial success and retains discoverable metadata")) {
        return 1;
    }

    // Repeated restore after successful cleanup cannot restore again or damage the first output.
    failureStep.clear();
    ArchiveRestoreResult repeated{};
    if (!check(!restoreFromArchive(vault.string(), metadata.archiveId,
                                   (temporary.path / "success-out").string(), repeated) &&
               repeated.errorCode == "archive_not_found" &&
               readBytes(success.restoredPath, actual) && actual == expected,
               "repeated restore reports the consumed archive without changing the restored file")) {
        return 1;
    }

    // Two simultaneous requests serialize: exactly one consumes the archive.
    fs::path concurrentVault;
    ArchiveMetadata concurrentMeta{};
    std::string concurrentBytes;
    if (!check(makeArchive(temporary.path, "concurrent", concurrentVault,
                           concurrentMeta, concurrentBytes),
               "concurrent restore fixture archived")) {
        return 1;
    }

    ArchiveRestoreResult first{}, second{};
    bool firstOk = false;
    bool secondOk = false;
    std::thread a([&] {
        firstOk = restoreFromArchive(
            concurrentVault.string(), concurrentMeta.archiveId,
            (temporary.path / "concurrent-out").string(), first
        );
    });
    std::thread b([&] {
        secondOk = restoreFromArchive(
            concurrentVault.string(), concurrentMeta.archiveId,
            (temporary.path / "concurrent-out").string(), second
        );
    });
    a.join();
    b.join();

    const ArchiveRestoreResult& winner = firstOk ? first : second;
    const ArchiveRestoreResult& loser = firstOk ? second : first;
    if (!check(firstOk != secondOk && winner.restored &&
               loser.errorCode == "archive_not_found",
               "concurrent restore requests serialize and only one consumes the archive")) {
        return 1;
    }
#endif

    // SHA-256 mismatch must fail before publication and preserve both artifacts.
    fs::path shaVault;
    ArchiveMetadata shaMeta{};
    std::string shaBytes;
    if (!check(makeArchive(temporary.path, "sha-mismatch", shaVault, shaMeta, shaBytes),
               "SHA mismatch fixture archived")) {
        return 1;
    }
    const fs::path shaMetadataPath = shaVault / "metadata" / (shaMeta.archiveId + ".meta");
    if (!loadArchiveMetadata(shaMetadataPath.string(), shaMeta)) return 1;
    shaMeta.originalSha256 = std::string(64, '0');
    shaMeta.hasOriginalSha256 = true;
    if (!check(saveArchiveMetadata(shaMetadataPath.string(), shaMeta),
               "SHA mismatch digest recorded in fixture metadata")) {
        return 1;
    }
    ArchiveRestoreResult shaFailure{};
    const fs::path shaOutput = temporary.path / "sha-output";
    if (!check(!restoreFromArchive(shaVault.string(), shaMeta.archiveId,
                                   shaOutput.string(), shaFailure) &&
               !shaFailure.restored && fs::exists(shaMeta.archivePath) &&
               fs::exists(shaMetadataPath) && !fs::exists(shaOutput),
               "SHA-256 mismatch preserves archive and prevents publication")) {
        return 1;
    }

    // Legacy metadata without a digest remains restorable.
    fs::path legacyVault;
    ArchiveMetadata legacyMeta{};
    std::string legacyBytes;
    if (!check(makeArchive(temporary.path, "legacy", legacyVault, legacyMeta, legacyBytes),
               "legacy fixture archived")) {
        return 1;
    }
    const fs::path legacyMetadataPath =
        legacyVault / "metadata" / (legacyMeta.archiveId + ".meta");
    if (!loadArchiveMetadata(legacyMetadataPath.string(), legacyMeta)) return 1;
    legacyMeta.hasOriginalSha256 = false;
    legacyMeta.originalSha256.clear();
    legacyMeta.hasOriginalPermissions = false;
    if (!check(saveArchiveMetadata(legacyMetadataPath.string(), legacyMeta),
               "legacy metadata rewritten without optional SHA-256 and permissions")) {
        return 1;
    }
    ArchiveRestoreResult legacy{};
    if (!check(restoreFromArchive(legacyVault.string(), legacyMeta.archiveId,
                                  (temporary.path / "legacy-out").string(), legacy) &&
               legacy.restored && legacy.archiveDeleted,
               "legacy archive restores and is cleaned up")) {
        return 1;
    }

    ArchiveRestoreResult invalid{};
    if (!check(!restoreFromArchive(legacyVault.string(), "../escape",
                                   (temporary.path / "invalid-out").string(), invalid) &&
               invalid.errorCode == "invalid_archive_id",
               "invalid archive ID is rejected before path construction")) {
        return 1;
    }

#ifdef SFV_ENABLE_TEST_HOOK
    setArchiveDeletionTestHook(nullptr);
#endif
    std::cout << "ALL RESTORE ARCHIVE CLEANUP TESTS PASSED\n";
    return 0;
}

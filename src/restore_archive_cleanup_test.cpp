#include "archive_service.h"
#include "file_manager.h"
#include "vault_service.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <set>
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

    // Scan the source parent after restore cleanup. The vault subtree must
    // remain excluded, while an ordinary similarly named sibling and the
    // published restore destination remain discoverable.
    const fs::path similarSibling = temporary.path / "success-vault-backup";
    if (!check(writeFixture(similarSibling / "ordinary.txt", actual),
               "similarly named ordinary sibling fixture created")) {
        return 1;
    }

    FileScanResult postRestoreScan{};
    if (!check(scanDirectory(
                   temporary.path.string(), 30, postRestoreScan, vault.string()),
               "scanner succeeds after restore cleanup")) {
        return 1;
    }

    std::set<std::string> postRestoreNames;
    for (const FileMetadata& item : postRestoreScan.files) {
        postRestoreNames.insert(item.fileName);
    }
    if (!check(postRestoreNames.count("success.bin") == 1 &&
               postRestoreNames.count("ordinary.txt") == 1 &&
               postRestoreNames.count("success.gz") == 0 &&
               postRestoreNames.count("success.meta") == 0,
               "post-restore scan sees restored and sibling files but no consumed vault artifacts")) {
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
    const bool metadataRestoreReturned = restoreFromArchive(
        metadataFailureVault.string(), metadataFailureMeta.archiveId,
        (temporary.path / "metadata-failure-out").string(), metadataFailure
    );
    const std::vector<ArchiveMetadata> partiallyCleanedCatalogue =
        getAllArchives(metadataFailureVault.string());
    if (!check(metadataRestoreReturned &&
               metadataFailure.restored && metadataFailure.partialFailure &&
               metadataFailure.compressedRemoved && !metadataFailure.metadataRemoved &&
               !fs::exists(metadataFailureMeta.archivePath) &&
               fs::exists(remainingMetadata) &&
               partiallyCleanedCatalogue.size() == 1 &&
               partiallyCleanedCatalogue[0].compressedSize == 0 &&
               fs::exists(metadataFailure.restoredPath),
               "metadata deletion failure keeps a discoverable record with zero remaining compressed bytes")) {
        return 1;
    }

    // A compressed-directory sync failure leaves the metadata catalogue entry.
    fs::path compressedSyncVault;
    ArchiveMetadata compressedSyncMeta{};
    std::string compressedSyncBytes;
    if (!check(makeArchive(temporary.path, "compressed-sync", compressedSyncVault,
                           compressedSyncMeta, compressedSyncBytes),
               "compressed-directory sync fixture archived")) {
        return 1;
    }
    failureStep = "restore_compressed_sync";
    ArchiveRestoreResult compressedSync{};
    if (!check(restoreFromArchive(
                   compressedSyncVault.string(), compressedSyncMeta.archiveId,
                   (temporary.path / "compressed-sync-out").string(), compressedSync) &&
               compressedSync.restored && compressedSync.partialFailure &&
               compressedSync.compressedRemoved && !compressedSync.metadataRemoved &&
               !fs::exists(compressedSyncMeta.archivePath) &&
               fs::exists(compressedSyncVault / "metadata" /
                          (compressedSyncMeta.archiveId + ".meta")) &&
               getAllArchives(compressedSyncVault.string()).size() == 1,
               "compressed-directory sync failure preserves discoverable metadata")) {
        return 1;
    }

    // A metadata-directory sync failure occurs after both names are removed.
    fs::path metadataSyncVault;
    ArchiveMetadata metadataSyncMeta{};
    std::string metadataSyncBytes;
    if (!check(makeArchive(temporary.path, "metadata-sync", metadataSyncVault,
                           metadataSyncMeta, metadataSyncBytes),
               "metadata-directory sync fixture archived")) {
        return 1;
    }
    failureStep = "restore_metadata_sync";
    ArchiveRestoreResult metadataSync{};
    if (!check(restoreFromArchive(
                   metadataSyncVault.string(), metadataSyncMeta.archiveId,
                   (temporary.path / "metadata-sync-out").string(), metadataSync) &&
               metadataSync.restored && metadataSync.partialFailure &&
               metadataSync.archiveDeleted && metadataSync.compressedRemoved &&
               metadataSync.metadataRemoved &&
               !fs::exists(metadataSyncMeta.archivePath) &&
               !fs::exists(metadataSyncVault / "metadata" /
                           (metadataSyncMeta.archiveId + ".meta")),
               "metadata-directory sync failure reports artifacts removed but partial durability status")) {
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
    const fs::path shaOutputDirectory = temporary.path / "sha-output";
    const fs::path shaOutput = shaOutputDirectory / "sha-mismatch.bin";
    if (!check(!restoreFromArchive(shaVault.string(), shaMeta.archiveId,
                                   shaOutputDirectory.string(), shaFailure) &&
               !shaFailure.restored && fs::exists(shaMeta.archivePath) &&
               fs::exists(shaMetadataPath) && !fs::exists(shaOutput),
               "SHA-256 mismatch preserves archive and prevents final-file publication")) {
        return 1;
    }

    // A pre-existing destination is never overwritten; the numbered name is used.
    fs::path collisionVault;
    ArchiveMetadata collisionMeta{};
    std::string collisionBytes;
    if (!check(makeArchive(temporary.path, "collision", collisionVault,
                           collisionMeta, collisionBytes),
               "destination collision fixture archived")) {
        return 1;
    }
    const fs::path collisionDestination = temporary.path / "collision-out";
    std::error_code collisionError;
    fs::create_directories(collisionDestination, collisionError);
    const fs::path occupiedPath = collisionDestination / "collision.bin";
    const std::string occupiedBytes = "Existing destination must remain untouched.";
    {
        std::ofstream occupied(occupiedPath, std::ios::binary);
        occupied << occupiedBytes;
    }
    ArchiveRestoreResult collision{};
    if (!check(restoreFromArchive(collisionVault.string(), collisionMeta.archiveId,
                                  collisionDestination.string(), collision) &&
               collision.restored &&
               collision.restoredPath ==
                   (collisionDestination / "collision (1).bin").string() &&
               readBytes(occupiedPath, actual) && actual == occupiedBytes &&
               readBytes(collision.restoredPath, actual) && actual == collisionBytes,
               "existing destination is preserved and restored file uses a numbered name")) {
        return 1;
    }

    // A truncated GZIP stream must fail before publication and preserve artifacts.
    fs::path corruptVault;
    ArchiveMetadata corruptMeta{};
    std::string corruptBytes;
    if (!check(makeArchive(temporary.path, "corrupt", corruptVault, corruptMeta, corruptBytes),
               "corrupt GZIP fixture archived")) {
        return 1;
    }
    const auto compressedSize = fs::file_size(corruptMeta.archivePath);
    fs::resize_file(corruptMeta.archivePath, compressedSize / 2);
    ArchiveRestoreResult corrupt{};
    const fs::path corruptDestination = temporary.path / "corrupt-out";
    if (!check(!restoreFromArchive(corruptVault.string(), corruptMeta.archiveId,
                                   corruptDestination.string(), corrupt) &&
               !corrupt.restored && fs::exists(corruptMeta.archivePath) &&
               fs::exists(corruptVault / "metadata" /
                          (corruptMeta.archiveId + ".meta")) &&
               !fs::exists(corruptDestination / "corrupt.bin"),
               "truncated GZIP failure preserves archive and metadata without publication")) {
        return 1;
    }

    // A metadata size larger than available space is rejected before decompression.
    fs::path spaceVault;
    ArchiveMetadata spaceMeta{};
    std::string spaceBytes;
    if (!check(makeArchive(temporary.path, "space", spaceVault, spaceMeta, spaceBytes),
               "insufficient-space fixture archived")) {
        return 1;
    }
    const fs::path spaceMetadataPath =
        spaceVault / "metadata" / (spaceMeta.archiveId + ".meta");
    if (!loadArchiveMetadata(spaceMetadataPath.string(), spaceMeta)) return 1;
    spaceMeta.originalSize = std::numeric_limits<std::uintmax_t>::max();
    spaceMeta.hasOriginalFileIdentity = false;
    if (!check(saveArchiveMetadata(spaceMetadataPath.string(), spaceMeta),
               "insufficient-space metadata fixture saved")) {
        return 1;
    }
    ArchiveRestoreResult noSpace{};
    if (!check(!restoreFromArchive(spaceVault.string(), spaceMeta.archiveId,
                                   (temporary.path / "space-out").string(), noSpace) &&
               !noSpace.restored && fs::exists(spaceMeta.archivePath) &&
               fs::exists(spaceMetadataPath),
               "insufficient restore space preserves archive and metadata")) {
        return 1;
    }

    // Destination validation failure also leaves the vault copy intact.
    fs::path destinationFailureVault;
    ArchiveMetadata destinationFailureMeta{};
    std::string destinationFailureBytes;
    if (!check(makeArchive(temporary.path, "destination-failure",
                           destinationFailureVault, destinationFailureMeta,
                           destinationFailureBytes),
               "destination-failure fixture archived")) {
        return 1;
    }
    const fs::path destinationIsFile = temporary.path / "not-a-directory";
    {
        std::ofstream file(destinationIsFile);
        file << "not a directory";
    }
    ArchiveRestoreResult destinationFailure{};
    if (!check(!restoreFromArchive(destinationFailureVault.string(),
                                   destinationFailureMeta.archiveId,
                                   destinationIsFile.string(), destinationFailure) &&
               !destinationFailure.restored &&
               fs::exists(destinationFailureMeta.archivePath) &&
               fs::exists(destinationFailureVault / "metadata" /
                          (destinationFailureMeta.archiveId + ".meta")),
               "invalid destination failure preserves archive and metadata")) {
        return 1;
    }

    // Missing compressed archive must not cause the remaining metadata to be deleted.
    fs::path missingCompressedVault;
    ArchiveMetadata missingCompressedMeta{};
    std::string missingCompressedBytes;
    if (!check(makeArchive(temporary.path, "missing-compressed",
                           missingCompressedVault, missingCompressedMeta,
                           missingCompressedBytes),
               "missing-compressed fixture archived")) {
        return 1;
    }
    fs::remove(missingCompressedMeta.archivePath);
    ArchiveRestoreResult missingCompressed{};
    if (!check(!restoreFromArchive(missingCompressedVault.string(),
                                   missingCompressedMeta.archiveId,
                                   (temporary.path / "missing-compressed-out").string(),
                                   missingCompressed) &&
               missingCompressed.errorCode == "archive_not_found" &&
               fs::exists(missingCompressedVault / "metadata" /
                          (missingCompressedMeta.archiveId + ".meta")),
               "missing compressed archive leaves its metadata untouched")) {
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

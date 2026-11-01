#include "archive.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

static fs::path createTestDirectory()
{
    char pattern[] = "/tmp/sfv-metadata-validation.XXXXXX";
    char* result = ::mkdtemp(pattern);
    return result == nullptr ? fs::path{} : fs::path(result);
}

static bool writeMetadata(
    const fs::path& path,
    const std::string& originalSize,
    const std::string& compressedSize
)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    output
        << "SFV-METADATA-VALIDATION\n"
        << "sample.txt\n"
        << "/legacy/sample.txt\n"
        << "/legacy/sample.txt.gz\n"
        << originalSize << '\n'
        << compressedSize << '\n'
        << "95.0\n"
        << "1730000000\n";

    output.close();
    return !output.fail();
}

static bool hasMetadataTemporaryAlias(const fs::path& root)
{
    std::error_code error;

    for (fs::directory_iterator it(root, error);
         !error && it != fs::directory_iterator();
         it.increment(error)) {
        const std::string name = it->path().filename().string();
        if (name.find(".sfv-meta-") != std::string::npos) {
            return true;
        }
    }

    return static_cast<bool>(error);
}

static bool readTextFile(const fs::path& path, std::string& contents)
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

static bool rejected(
    const fs::path& root,
    const std::string& name,
    const std::string& originalSize,
    const std::string& compressedSize
)
{
    const fs::path path = root / (name + ".meta");
    if (!writeMetadata(path, originalSize, compressedSize)) {
        std::cerr << "FAIL: could not write metadata fixture " << name << '\n';
        return false;
    }

    ArchiveMetadata metadata {};
    return !loadArchiveMetadata(path.string(), metadata);
}

static bool accepted(
    const fs::path& root,
    const std::string& name,
    const std::string& originalSize,
    const std::string& compressedSize
)
{
    const fs::path path = root / (name + ".meta");
    if (!writeMetadata(path, originalSize, compressedSize)) {
        return false;
    }

    ArchiveMetadata metadata {};
    return loadArchiveMetadata(path.string(), metadata) &&
        metadata.originalSize == 5 &&
        metadata.compressedSize == 3;
}

int main()
{
    const fs::path root = createTestDirectory();
    if (root.empty()) {
        std::cerr << "FAIL: could not create isolated test directory\n";
        return 1;
    }

    bool passed = true;
    const std::string overflow(128, '9');

    const struct {
        const char* name;
        const std::string original;
        const std::string compressed;
    } invalidCases[] = {
        {"negative-original", "-1", "3"},
        {"signed-original", "+5", "3"},
        {"leading-space-original", " 5", "3"},
        {"trailing-space-original", "5 ", "3"},
        {"leading-tab-original", "\t5", "3"},
        {"trailing-tab-original", "5\t", "3"},
        {"carriage-return-original", "5\r", "3"},
        {"empty-original", "", "3"},
        {"overflow-original", overflow, "3"},
        {"negative-compressed", "5", "-1"},
        {"signed-compressed", "5", "+3"},
        {"leading-space-compressed", "5", " 3"},
        {"trailing-space-compressed", "5", "3 "},
        {"leading-tab-compressed", "5", "\t3"},
        {"trailing-tab-compressed", "5", "3\t"},
        {"carriage-return-compressed", "5", "3\r"},
        {"empty-compressed", "5", ""},
        {"overflow-compressed", "5", overflow}
    };

    for (const auto& test : invalidCases) {
        const bool casePassed =
            rejected(root, test.name, test.original, test.compressed);
        std::cout
            << (casePassed ? "PASS: " : "FAIL: ")
            << test.name << '\n';
        passed = casePassed && passed;
    }

    const bool validPassed =
        accepted(root, "valid-decimal-sizes", "5", "3");
    std::cout
        << (validPassed ? "PASS: " : "FAIL: ")
        << "valid ASCII decimal sizes accepted\n";
    passed = validPassed && passed;

#ifdef SFV_ENABLE_TEST_HOOKS
    ArchiveMetadata publicationMetadata {};
    publicationMetadata.archiveId = "SFV-METADATA-PUBLICATION-1";
    publicationMetadata.originalName = "sample.txt";
    publicationMetadata.originalPath = "/legacy/sample.txt";
    publicationMetadata.archivePath = "/legacy/sample.txt.gz";
    publicationMetadata.originalSize = 5;
    publicationMetadata.compressedSize = 3;
    publicationMetadata.compressionPercentage = 40.0;
    publicationMetadata.archivedAt = 1730000000;

    const fs::path fileSyncFailurePath = root / "file-sync-failure.meta";
    bool publicationMayRemain = false;
    setArchiveMetadataPublicationFailureInjection(true, false, false);
    const bool fileSyncFailureRejected =
        !saveArchiveMetadata(
            fileSyncFailurePath.string(),
            publicationMetadata,
            &publicationMayRemain
        ) &&
        !publicationMayRemain &&
        !fs::exists(fileSyncFailurePath) &&
        !hasMetadataTemporaryAlias(root);
    std::cout
        << (fileSyncFailureRejected ? "PASS: " : "FAIL: ")
        << "metadata file-sync failure removes partial temporary record\\n";
    passed = fileSyncFailureRejected && passed;

    const fs::path directorySyncFailurePath = root / "directory-sync-failure.meta";
    publicationMayRemain = false;
    setArchiveMetadataPublicationFailureInjection(false, true, false);
    const bool directorySyncRollback =
        !saveArchiveMetadata(
            directorySyncFailurePath.string(),
            publicationMetadata,
            &publicationMayRemain
        ) &&
        !publicationMayRemain &&
        !fs::exists(directorySyncFailurePath) &&
        !hasMetadataTemporaryAlias(root);
    std::cout
        << (directorySyncRollback ? "PASS: " : "FAIL: ")
        << "metadata directory-sync failure rolls back and cleans the record\\n";
    passed = directorySyncRollback && passed;

    const fs::path rollbackFailurePath = root / "rollback-failure.meta";
    publicationMayRemain = false;
    setArchiveMetadataPublicationFailureInjection(false, true, true);
    const bool rollbackFailureReturned =
        !saveArchiveMetadata(
            rollbackFailurePath.string(),
            publicationMetadata,
            &publicationMayRemain
        ) &&
        publicationMayRemain &&
        fs::exists(rollbackFailurePath) &&
        !hasMetadataTemporaryAlias(root);

    ArchiveMetadata retainedMetadata {};
    const bool retainedRecordLoads =
        rollbackFailureReturned &&
        loadArchiveMetadata(rollbackFailurePath.string(), retainedMetadata) &&
        retainedMetadata.archiveId == publicationMetadata.archiveId &&
        retainedMetadata.originalSize == publicationMetadata.originalSize &&
        retainedMetadata.compressedSize == publicationMetadata.compressedSize;

    std::cout
        << (retainedRecordLoads ? "PASS: " : "FAIL: ")
        << "rollback-unlink failure reports and retains the complete metadata record\\n";
    passed = retainedRecordLoads && passed;

    std::error_code publicationCleanupError;
    fs::remove(rollbackFailurePath, publicationCleanupError);
    if (publicationCleanupError) {
        std::cerr
            << "FAIL: could not clean retained rollback fixture: "
            << publicationCleanupError.message() << '\\n';
        passed = false;
    }

    const fs::path collisionPath = root / "existing.meta";
    const std::string collisionSentinel = "existing metadata must remain unchanged\\n";
    {
        std::ofstream sentinel(collisionPath, std::ios::binary);
        sentinel << collisionSentinel;
        sentinel.close();
        if (sentinel.fail()) {
            std::cerr << "FAIL: could not write metadata collision sentinel\\n";
            passed = false;
        }
    }

    std::string collisionAfter;
    publicationMayRemain = false;
    const bool collisionNotOverwritten =
        !publicationMayRemain &&
        !saveArchiveMetadata(
            collisionPath.string(),
            publicationMetadata,
            &publicationMayRemain
        ) &&
        !publicationMayRemain &&
        readTextFile(collisionPath, collisionAfter) &&
        collisionAfter == collisionSentinel &&
        !hasMetadataTemporaryAlias(root);
    std::cout
        << (collisionNotOverwritten ? "PASS: " : "FAIL: ")
        << "metadata publication does not overwrite an existing record and removes its temp alias\\n";
    passed = collisionNotOverwritten && passed;

    const fs::path successfulPublicationPath = root / "successful-publication.meta";
    const bool successfulPublication =
        saveArchiveMetadata(
            successfulPublicationPath.string(),
            publicationMetadata
        ) &&
        !hasMetadataTemporaryAlias(root);
    ArchiveMetadata successfulMetadata {};
    const bool successfulPublicationLoads =
        successfulPublication &&
        loadArchiveMetadata(
            successfulPublicationPath.string(),
            successfulMetadata
        ) &&
        successfulMetadata.archiveId == publicationMetadata.archiveId;
    std::cout
        << (successfulPublicationLoads ? "PASS: " : "FAIL: ")
        << "atomic metadata publication writes and reloads a complete record\\n";
    passed = successfulPublicationLoads && passed;
#endif

    if (passed) {
        std::error_code cleanupError;
        fs::remove_all(root, cleanupError);
        if (cleanupError) {
            std::cerr << "FAIL: cleanup failed: "
                      << cleanupError.message() << '\n';
            passed = false;
        }
    }

    if (!passed) {
        std::cerr
            << "METADATA VALIDATION TESTS FAILED; preserving fixtures at "
            << root.string() << '\n';
        return 1;
    }

    std::cout << "ALL METADATA VALIDATION TESTS PASSED\n";
    return 0;
}

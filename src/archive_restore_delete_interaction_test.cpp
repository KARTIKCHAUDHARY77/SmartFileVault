
#include "vault_service.h"
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

static fs::path makeTemporaryDirectory()
{
    std::error_code error;
    fs::path base = fs::temp_directory_path(error);

    if (error) {
        return {};
    }

    std::string pattern =
        (base / "sfv_restore_delete_interaction.XXXXXX").string();

    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');

    char* created = ::mkdtemp(writable.data());

    return created == nullptr ? fs::path{} : fs::path(created);
}

static bool writeReferenceFile(
    const fs::path& path,
    std::string& contents
)
{
    const std::string line =
        "SmartFileVault restore-delete interaction test data.\n";

    contents.clear();

    for (int i = 0; i < 3000; ++i) {
        contents += line;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);

    if (!output) {
        return false;
    }

    output.write(
        contents.data(),
        static_cast<std::streamsize>(contents.size())
    );

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

static bool check(bool passed, const char* description)
{
    std::cout << (passed ? "PASS: " : "FAIL: ")
              << description << '\n';

    return passed;
}

int main()
{
    TemporaryDirectory temporary;
    temporary.path = makeTemporaryDirectory();

    if (!check(
            !temporary.path.empty(),
            "temporary test directory created")) {
        return 1;
    }

    const fs::path sourceDirectory =
        temporary.path / "source";

    const fs::path vaultRoot =
        temporary.path / "vault";

    const fs::path referencePath =
        temporary.path / "reference-copy.bin";

    const fs::path originalPath =
        sourceDirectory / "interaction-test.txt";

    std::error_code error;
    fs::create_directories(sourceDirectory, error);

    if (!check(
            !error,
            "source directory created")) {
        return 1;
    }

    std::string referenceBytes;

    if (!check(
            writeReferenceFile(originalPath, referenceBytes),
            "disposable source file created")) {
        return 1;
    }

    fs::copy_file(
        originalPath,
        referencePath,
        fs::copy_options::none,
        error
    );

    if (!check(
            !error,
            "independent reference copy created")) {
        return 1;
    }

    std::string savedReference;

    if (!check(
            readFile(referencePath, savedReference) &&
            savedReference == referenceBytes,
            "reference copy matches source bytes")) {
        return 1;
    }

    // 1. Archive the source and remove its original.
    ArchiveMetadata metadata{};

    if (!check(
            archiveAndRemoveOriginal(
                originalPath.string(),
                vaultRoot.string(),
                metadata
            ),
            "source successfully archived")) {
        return 1;
    }

    if (!check(
            !fs::exists(originalPath),
            "original source removed after archiving")) {
        return 1;
    }

    // 2. Restore into the original directory.
    // This recreates the original pathname with a new file identity.
    std::string restoredPath;

    if (!check(
            restoreFromArchive(
                vaultRoot.string(),
                metadata.archiveId,
                sourceDirectory.string(),
                restoredPath
            ),
            "archive restored through vault service")) {
        return 1;
    }

    std::string restoredBytes;

    if (!check(
            restoredPath == originalPath.string() &&
            readFile(restoredPath, restoredBytes) &&
            restoredBytes == savedReference,
            "restored bytes exactly match independent reference copy")) {
        return 1;
    }

    // 3. Simulate a user editing the restored file.
    const std::string userEdit =
        "\nUser edit made after restoring this file.\n";

    {
        std::ofstream output(
            restoredPath,
            std::ios::binary | std::ios::app
        );

        if (!output) {
            return check(false, "restored file opened for user edit") ? 0 : 1;
        }

        output.write(
            userEdit.data(),
            static_cast<std::streamsize>(userEdit.size())
        );

        output.close();

        if (output.fail()) {
            return check(false, "user edit successfully written") ? 0 : 1;
        }
    }

    const std::string expectedEditedBytes =
        savedReference + userEdit;

    std::string editedBytes;

    if (!check(
            readFile(restoredPath, editedBytes) &&
            editedBytes == expectedEditedBytes,
            "user edit successfully persisted")) {
        return 1;
    }

    // 4. Restore automatically consumes only the vault artifacts.
    const fs::path metadataPath =
        vaultRoot / "metadata" / (metadata.archiveId + ".meta");
    if (!check(
            !fs::exists(metadata.archivePath) && !fs::exists(metadataPath),
            "restore removes compressed archive and metadata")) {
        return 1;
    }

    // 5. Verify the restored and subsequently edited destination is untouched.
    std::string finalBytes;
    if (!check(
            fs::exists(restoredPath) &&
            readFile(restoredPath, finalBytes) &&
            finalBytes == expectedEditedBytes,
            "restored and user-edited destination survives automatic cleanup")) {
        return 1;
    }

    // 6. Verify Delete Both independently on a second disposable archive.
    const fs::path deleteBothSource = sourceDirectory / "delete-both.txt";
    if (!check(
            writeReferenceFile(deleteBothSource, referenceBytes),
            "independent Delete Both fixture created")) {
        return 1;
    }

    ArchiveMetadata deleteBothMetadata{};
    if (!check(
            createArchive(
                deleteBothSource.string(),
                vaultRoot.string(),
                deleteBothMetadata
            ),
            "independent Delete Both archive created")) {
        return 1;
    }

    ArchiveDeletionResult deletion{};
    const bool deletionSucceeded = deleteArchiveFromVault(
        vaultRoot.string(), deleteBothMetadata.archiveId, deletion
    );
    if (!check(
            deletionSucceeded && deletion.archiveDeleted &&
            deletion.compressedRemoved && deletion.metadataRemoved &&
            deletion.originalDeleted && !fs::exists(deleteBothSource),
            "Delete Both still removes its independently verified original and artifacts")) {
        return 1;
    }

    // 7. Verify both completed operations leave no catalogue entries.
    const std::vector<ArchiveMetadata> remainingArchives =
        getAllArchives(vaultRoot.string());
    if (!check(
            remainingArchives.empty(),
            "completed archives absent from vault catalogue")) {
        return 1;
    }

    std::cout << "\nAll archive-restore-delete interaction tests passed.\n";

    return 0;
}

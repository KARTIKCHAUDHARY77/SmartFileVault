#include "archive.h"
#include "test_support.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

// Recursively looks for any regular file under `folder`.
// Returns false only on a filesystem error.
static bool findRegularFile(
    const fs::path& folder,
    bool& found,
    std::string& firstFound
) {
    found = false;
    std::error_code error;

    if (!fs::exists(folder, error)) {
        return !error;
    }

    fs::recursive_directory_iterator it(folder, error);
    const fs::recursive_directory_iterator end;

    if (error) {
        return false;
    }

    while (it != end) {
        const bool isFile = it->is_regular_file(error);

        if (error) {
            return false;
        }

        if (isFile) {
            found = true;
            firstFound = it->path().string();
            return true;
        }

        it.increment(error);

        if (error) {
            return false;
        }
    }

    return true;
}

int main() {
    testsupport::TestDirectory dir(
        "smartfilevault_incompressible_test");

    if (!dir.ok()) {
        std::cerr << "FAIL: Could not create test directory\n";
        return 1;
    }

    const fs::path original = dir.path() / "random_data.bin";
    const fs::path archiveFolder = dir.path() / "archive";

    const std::vector<unsigned char> originalBytes =
        testsupport::pseudoRandomBytes(100000, 12345);

    if (!testsupport::writeAllBytes(original, originalBytes)) {
        std::cerr << "FAIL: Could not create original file\n";
        return 1;
    }

    int failures = 0;

    // Control: a compressible file must archive successfully in the
    // same kind of setup. If this fails, the rejection checks below
    // cannot be trusted, because rejection might be for another reason.
    {
        const fs::path controlFile = dir.path() / "control.txt";
        const fs::path controlArchive = dir.path() / "archive_control";

        if (!testsupport::writeAllBytes(
                controlFile,
                testsupport::repeatedPatternBytes(100000))) {
            std::cerr << "FAIL: Could not create control file\n";
            return 1;
        }

        ArchiveMetadata controlMetadata{};

        const bool controlArchived = createArchive(
            controlFile.string(),
            controlArchive.string(),
            controlMetadata
        );

        bool found = false;
        std::string firstFound;

        if (controlArchived &&
            findRegularFile(controlArchive, found, firstFound) &&
            found) {
            std::cout << "PASS: Control - compressible file archived\n";
        } else {
            std::cerr << "FAIL: Control - compressible file was not "
                         "archived; results below are not trustworthy\n";
            ++failures;
        }
    }

    ArchiveMetadata metadata{};

    const bool archived = createArchive(
        original.string(),
        archiveFolder.string(),
        metadata
    );

    if (!archived) {
        std::cout << "PASS: Incompressible file rejected\n";
    } else {
        std::cerr << "FAIL: Incompressible file was archived\n";
        ++failures;
    }

    std::error_code error;
    const bool originalExists = fs::exists(original, error);

    if (originalExists && !error) {
        std::cout << "PASS: Original file preserved\n";
    } else {
        std::cerr << "FAIL: Original file missing\n";
        ++failures;
    }

    // Byte-for-byte check, not just size.
    std::vector<unsigned char> afterBytes;

    if (originalExists &&
        testsupport::readAllBytes(original, afterBytes) &&
        afterBytes == originalBytes) {
        std::cout << "PASS: Original content unchanged (byte-for-byte)\n";
    } else {
        std::cerr << "FAIL: Original content changed or unreadable\n";
        ++failures;
    }

    // Scan the WHOLE archive folder, not one assumed subfolder.
    bool found = false;
    std::string firstFound;

    if (!findRegularFile(archiveFolder, found, firstFound)) {
        std::cerr << "FAIL: Could not scan archive folder\n";
        ++failures;
    } else if (!found) {
        std::cout << "PASS: No archive files retained\n";
    } else {
        std::cerr << "FAIL: Archive file retained: "
                  << firstFound << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << "Incompressible-file tests failed: "
                  << failures << '\n';
        return 1;
    }

    if (!dir.cleanup()) {
        return 1;
    }
    std::cout << "All incompressible-file tests passed.\n";
    return 0;
}
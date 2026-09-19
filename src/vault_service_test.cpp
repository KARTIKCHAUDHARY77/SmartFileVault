#include "archive.h"
#include "file_utils.h"
#include "vault_service.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void createTestFile(
    const std::string& path
)
{
    std::ofstream file(path);

    if (!file) {
        return;
    }

    for (int i = 0; i < 5000; ++i) {
        file << "SmartFileVault end to end test data.\n";
    }
}

int main()
{
    const std::string source =
        "vault_service_test.txt";

    const std::string reference =
        "vault_service_reference.txt";

    const std::string archiveFolder =
        "vault_service_archive";

    const std::string restoreFolder =
        "vault_service_restore";

    createTestFile(source);

    fs::copy_file(
        source,
        reference,
        fs::copy_options::overwrite_existing
    );

    ArchiveMetadata metadata;

    if (!processCandidate(
            source,
            archiveFolder,
            metadata
        )) {

        std::cout
            << "Candidate processing: FAIL\n";

        fs::remove(reference);
        fs::remove_all(archiveFolder);

        return 1;
    }

    std::cout
        << "Candidate processing: PASS\n";


    if (!fs::exists(source)) {

        std::cout
            << "Original removed safely: PASS\n";

    } else {

        std::cout
            << "Original removed safely: FAIL\n";
    }


    VaultSummary summary;

    if (!getVaultSummary(
            archiveFolder,
            summary
        )) {

        std::cout
            << "Vault summary: FAIL\n";

        fs::remove(reference);
        fs::remove_all(archiveFolder);

        return 1;
    }


    std::cout
        << "Archive count: "
        << summary.archiveCount
        << '\n';

    std::cout
        << "Original storage: "
        << summary.originalSize
        << " bytes\n";

    std::cout
        << "Compressed storage: "
        << summary.compressedSize
        << " bytes\n";

    std::cout
        << "Space saved: "
        << summary.spaceSaved
        << " bytes\n";

    std::cout
        << "Restore safety space: "
        << summary.restoreSafetySpace
        << " bytes\n";


    if (summary.archiveCount == 1 &&
        summary.spaceSaved > 0) {

        std::cout
            << "Vault summary: PASS\n";

    } else {

        std::cout
            << "Vault summary: FAIL\n";
    }


    fs::create_directories(
        restoreFolder
    );


    std::string restoredPath;

    if (!restoreFromArchive(
            archiveFolder,
            metadata.archiveId,
            restoreFolder,
            restoredPath
        )) {

        std::cout
            << "Restore from archive: FAIL\n";

        fs::remove(reference);
        fs::remove_all(archiveFolder);
        fs::remove_all(restoreFolder);

        return 1;
    }


    std::cout
        << "Restore from archive: PASS\n";

    std::cout
        << "Restored path: "
        << restoredPath
        << '\n';


    if (!filesAreDifferent(
            reference,
            restoredPath
        )) {

        std::cout
            << "Restored data verification: PASS\n";

    } else {

        std::cout
            << "Restored data verification: FAIL\n";
    }


    /*
     * Test duplicate filename protection.
     *
     * The first restored file already exists,
     * so another restore should get a new name.
     */
    std::string secondRestoredPath;

    if (restoreFromArchive(
            archiveFolder,
            metadata.archiveId,
            restoreFolder,
            secondRestoredPath
        )) {

        std::cout
            << "Second restore: PASS\n";

        std::cout
            << "Second path: "
            << secondRestoredPath
            << '\n';

    } else {

        std::cout
            << "Second restore: FAIL\n";
    }


    fs::remove(reference);
    fs::remove_all(archiveFolder);
    fs::remove_all(restoreFolder);

    return 0;
}
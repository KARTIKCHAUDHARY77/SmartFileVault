#include "archive.h"
#include "vault_service.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
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
    if (error) return {};
    std::string pattern = (base / "sfv_delete_concurrency.XXXXXX").string();
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');
    char* created = ::mkdtemp(writable.data());
    return created ? fs::path(created) : fs::path{};
}

static bool writeFixture(const fs::path& path)
{
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    for (int i = 0; i < 1200; ++i) {
        file << "Concurrent SmartFileVault deletion fixture.\n";
    }
    file.close();
    return !file.fail();
}

int main()
{
    TemporaryDirectory temporary{makeTemporaryDirectory()};
    if (temporary.path.empty()) {
        std::cerr << "FAIL: could not create test directory\n";
        return 1;
    }

    const fs::path original = temporary.path / "source" / "race.txt";
    const fs::path vault = temporary.path / "vault";
    if (!writeFixture(original)) {
        std::cerr << "FAIL: could not create source fixture\n";
        return 1;
    }

    ArchiveMetadata metadata{};
    if (!createArchive(original.string(), vault.string(), metadata)) {
        std::cerr << "FAIL: could not create archive fixture\n";
        return 1;
    }

    ArchiveDeletionResult first{};
    ArchiveDeletionResult second{};
    std::atomic<bool> firstReturned{false};
    std::atomic<bool> secondReturned{false};
    bool firstSuccess = false;
    bool secondSuccess = false;

    std::thread firstThread([&] {
        firstSuccess = deleteArchiveFromVault(
            vault.string(), metadata.archiveId, first
        );
        firstReturned.store(true);
    });
    std::thread secondThread([&] {
        secondSuccess = deleteArchiveFromVault(
            vault.string(), metadata.archiveId, second
        );
        secondReturned.store(true);
    });

    firstThread.join();
    secondThread.join();

    const bool exactlyOneSucceeded = firstSuccess != secondSuccess;
    const ArchiveDeletionResult& failedResult =
        firstSuccess ? second : first;
    const bool loserSawNotFound =
        failedResult.errorCode == "archive_not_found";

    std::error_code error;
    const bool noArtifactsRemain =
        !fs::exists(metadata.archivePath, error) && !error &&
        !fs::exists(vault / "metadata" / (metadata.archiveId + ".meta"), error) && !error &&
        !fs::exists(original, error) && !error;

    const bool passed = firstReturned.load() && secondReturned.load() &&
                        exactlyOneSucceeded && loserSawNotFound && noArtifactsRemain;

    std::cout << (passed ? "PASS: " : "FAIL: ")
              << "concurrent deletion requests serialize; one succeeds and one safely reports archive_not_found\n";
    std::cout << (passed ? "ALL ARCHIVE DELETION CONCURRENCY TESTS PASSED\n"
                         : "ARCHIVE DELETION CONCURRENCY TESTS FAILED\n");
    return passed ? 0 : 1;
}

#include "archive.h"
#include "restore.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <stdlib.h>
#include <sys/stat.h>
#include <system_error>
#include <zlib.h>

namespace fs = std::filesystem;

static fs::path createTestDirectory()
{
    char pattern[] = "/tmp/sfv-legacy-metadata.XXXXXX";
    char* result = ::mkdtemp(pattern);
    return result == nullptr ? fs::path{} : fs::path(result);
}

static bool createLegacyGzip(
    const fs::path& archivePath,
    const std::string& payload
)
{
    // This uses the original zlib gzopen/gzwrite GZIP writer format used
    // by the pre-bounded decompression implementation.
    gzFile output = ::gzopen(archivePath.c_str(), "wb");
    if (output == nullptr) {
        return false;
    }

    const int written = ::gzwrite(
        output,
        payload.data(),
        static_cast<unsigned int>(payload.size())
    );
    const int closeResult = ::gzclose(output);

    return written == static_cast<int>(payload.size()) &&
           closeResult == Z_OK;
}

static bool readFile(const fs::path& path, std::string& contents)
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

static bool hasMode(const fs::path& path, unsigned int expected)
{
    struct stat status {};
    return ::stat(path.c_str(), &status) == 0 &&
        static_cast<unsigned int>(status.st_mode & 0777) == expected;
}

int main()
{
    const fs::path root = createTestDirectory();
    if (root.empty()) {
        std::cerr << "FAIL: could not create isolated legacy fixture directory\n";
        return 1;
    }

    const fs::path archivePath = root / "legacy-before-bounds.gz";
    const fs::path metadataPath = root / "legacy-before-bounds.meta";
    const fs::path restoreDirectory = root / "restored";
    const std::string payload =
        "SmartFileVault legacy archive fixture\n"
        "Created with the pre-bounded zlib GZIP writer.\n"
        "This small file verifies old eight-line metadata compatibility.\n";

    std::error_code error;
    if (!fs::create_directory(restoreDirectory, error) || error) {
        std::cerr << "FAIL: could not create restore directory\n";
        return 1;
    }

    if (!createLegacyGzip(archivePath, payload)) {
        std::cerr << "FAIL: could not create legacy GZIP fixture\n";
        return 1;
    }

    const std::uintmax_t compressedSize =
        fs::file_size(archivePath, error);
    if (error) {
        std::cerr << "FAIL: could not inspect legacy GZIP fixture\n";
        return 1;
    }

    // The original eight-line metadata format has no permissions line.
    {
        std::ofstream metadata(metadataPath, std::ios::binary);
        if (!metadata) {
            std::cerr << "FAIL: could not create legacy metadata\n";
            return 1;
        }

        metadata
            << "SFV-LEGACY-FIXTURE-1\n"
            << "legacy.txt\n"
            << "/legacy/source/legacy.txt\n"
            << archivePath.string() << '\n'
            << payload.size() << '\n'
            << compressedSize << '\n'
            << "0\n"
            << "1730000000\n";

        metadata.close();
        if (metadata.fail()) {
            std::cerr << "FAIL: could not finish legacy metadata\n";
            return 1;
        }
    }

    ArchiveMetadata loaded {};
    const bool metadataLoaded =
        loadArchiveMetadata(metadataPath.string(), loaded) &&
        !loaded.hasOriginalPermissions &&
        loaded.originalPermissions == 0600U &&
        loaded.originalSize == payload.size() &&
        loaded.compressedSize == compressedSize;

    if (!metadataLoaded) {
        std::cerr << "FAIL: eight-line legacy metadata was not accepted\n";
        return 1;
    }
    std::cout << "PASS: eight-line legacy metadata loads with 0600 fallback\n";

    std::string restoredPath;
    if (!restoreArchive(
            metadataPath.string(),
            restoreDirectory.string(),
            restoredPath)) {
        std::cerr << "FAIL: legacy GZIP archive could not be restored\n";
        return 1;
    }

    std::string restoredBytes;
    const bool exactBytes =
        readFile(restoredPath, restoredBytes) &&
        restoredBytes == payload;
    if (!exactBytes) {
        std::cerr << "FAIL: restored legacy bytes differ from fixture\n";
        return 1;
    }
    std::cout << "PASS: legacy GZIP archive restores byte-for-byte\n";

    if (!hasMode(restoredPath, 0600U)) {
        std::cerr << "FAIL: legacy restore did not use restrictive 0600 mode\n";
        return 1;
    }
    std::cout << "PASS: legacy restore uses 0600 permissions\n";

    fs::remove_all(root, error);
    if (error) {
        std::cerr
            << "FAIL: legacy fixture cleanup failed: "
            << error.message() << '\n';
        return 1;
    }

    std::cout << "ALL LEGACY METADATA COMPATIBILITY TESTS PASSED\n";
    return 0;
}

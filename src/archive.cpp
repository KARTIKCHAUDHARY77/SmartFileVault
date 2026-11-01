#include "archive.h"
#include "compression.h"

#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

#ifdef SFV_ENABLE_TEST_HOOKS
static bool g_testFailMetadataFileSync = false;
static bool g_testFailMetadataDirectorySync = false;
static bool g_testFailMetadataRollbackUnlink = false;

void setArchiveMetadataPublicationFailureInjection(
    bool failFileSync,
    bool failDirectorySync,
    bool failRollbackUnlink)
{
    g_testFailMetadataFileSync = failFileSync;
    g_testFailMetadataDirectorySync = failDirectorySync;
    g_testFailMetadataRollbackUnlink = failRollbackUnlink;
}
#endif

static bool syncFileDescriptor(int descriptor)
{
#if defined(__APPLE__) && defined(F_FULLFSYNC)
    if (::fcntl(descriptor, F_FULLFSYNC) == 0) {
        return true;
    }

    if (errno != EINVAL && errno != ENOTSUP && errno != ENOTTY) {
        return false;
    }
#endif

    return ::fsync(descriptor) == 0;
}

static bool syncDirectoryPath(const fs::path& path)
{
    const int descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor < 0) {
        return false;
    }

    bool success = (::fsync(descriptor) == 0);
    int savedError = errno;

    if (::close(descriptor) != 0 && success) {
        success = false;
        savedError = errno;
    }

    if (!success) {
        errno = savedError;
    }

    return success;
}

static bool parseUnsignedSize(
    const std::string& rawText,
    std::uintmax_t& parsedValue
)
{
    if (rawText.empty()) {
        return false;
    }

    const std::uintmax_t maximum =
        std::numeric_limits<std::uintmax_t>::max();
    std::uintmax_t value = 0;

    // Validate the original bytes. Do not trim, accept signs, or let a
    // conversion routine silently skip whitespace before parsing.
    for (unsigned char character : rawText) {
        if (character < '0' || character > '9') {
            return false;
        }

        const std::uintmax_t digit =
            static_cast<std::uintmax_t>(character - '0');
        if (value > (maximum - digit) / 10U) {
            return false;
        }

        value = value * 10U + digit;
    }

    parsedValue = value;
    return true;
}

static bool isUnsignedDecimal(const std::string& value)
{
    if (value.empty()) {
        return false;
    }

    for (unsigned char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
    }

    return true;
}

static std::string makeArchiveId()
{
    auto now =
        std::chrono::system_clock::now();

    auto time =
        std::chrono::duration_cast<
            std::chrono::milliseconds
        >(
            now.time_since_epoch()
        ).count();

    static int counter = 0;
    ++counter;

    std::ostringstream id;

    id << "SFV-"
       << time
       << "-"
       << counter;

    return id.str();
}

static std::string fileNameFromPath(
    const std::string& filePath
)
{
    return fs::path(filePath)
        .filename()
        .string();
}

bool saveArchiveMetadata(
    const std::string& metadataPath,
    const ArchiveMetadata& metadata,
    bool* publishedPathMayRemain
)
{
    if (publishedPathMayRemain != nullptr) {
        *publishedPathMayRemain = false;
    }

    // Serialize the complete record before creating a temporary file. The
    // final .meta path appears only through an atomic no-overwrite link.
    std::ostringstream serialized;
    serialized << metadata.archiveId << '\n';
    serialized << metadata.originalName << '\n';
    serialized << metadata.originalPath << '\n';
    serialized << metadata.archivePath << '\n';
    serialized << metadata.originalSize << '\n';
    serialized << metadata.compressedSize << '\n';
    serialized << metadata.compressionPercentage << '\n';
    serialized << metadata.archivedAt << '\n';
    if (metadata.hasOriginalPermissions) {
        serialized << metadata.originalPermissions << '\n';
    }

    const std::string contents = serialized.str();
    fs::path finalPath(metadataPath);
    fs::path parent = finalPath.parent_path();
    if (parent.empty()) {
        parent = ".";
    }

    const std::string pattern =
        (parent / (finalPath.filename().string() + ".sfv-meta-XXXXXX")).string();
    std::vector<char> writablePattern(pattern.begin(), pattern.end());
    writablePattern.push_back('\0');

    const int descriptor = ::mkstemp(writablePattern.data());
    if (descriptor < 0) {
        std::cerr << "Metadata error: could not create temporary metadata.\n";
        return false;
    }

    const std::string temporaryPath(writablePattern.data());
    FILE* output = ::fdopen(descriptor, "wb");
    if (output == nullptr) {
        const int savedError = errno;
        (void)::close(descriptor);
        (void)::unlink(temporaryPath.c_str());
        errno = savedError;
        std::cerr << "Metadata error: could not open temporary metadata stream.\n";
        return false;
    }

    bool success = true;
    if (!contents.empty() &&
        std::fwrite(contents.data(), 1, contents.size(), output) != contents.size()) {
        success = false;
    }

    if (success && std::fflush(output) != 0) {
        success = false;
    }

    if (success) {
        bool fileSynchronized = false;
#ifdef SFV_ENABLE_TEST_HOOKS
        if (g_testFailMetadataFileSync) {
            g_testFailMetadataFileSync = false;
            errno = EIO;
        } else
#endif
        {
            fileSynchronized =
                syncFileDescriptor(::fileno(output));
        }

        if (!fileSynchronized) {
            success = false;
        }
    }

    if (std::fclose(output) != 0) {
        success = false;
    }

    if (!success) {
        const int savedError = errno;
        if (::unlink(temporaryPath.c_str()) != 0 && errno != ENOENT) {
            std::cerr
                << "Metadata cleanup warning: partial temporary metadata remains at '"
                << temporaryPath << "'.\n";
        }
        errno = savedError;
        std::cerr << "Metadata error: could not write and sync metadata.\n";
        return false;
    }

    if (::link(temporaryPath.c_str(), metadataPath.c_str()) != 0) {
        const int savedError = errno;
        if (::unlink(temporaryPath.c_str()) != 0 && errno != ENOENT) {
            std::cerr
                << "Metadata cleanup warning: temporary metadata remains at '"
                << temporaryPath << "'.\n";
        }
        errno = savedError;
        return false;
    }

    bool metadataDirectorySynced = false;
#ifdef SFV_ENABLE_TEST_HOOKS
    if (g_testFailMetadataDirectorySync) {
        g_testFailMetadataDirectorySync = false;
        errno = EIO;
    } else
#endif
    {
        metadataDirectorySynced = syncDirectoryPath(parent);
    }

    if (!metadataDirectorySynced) {
        const int syncError = errno == 0 ? EIO : errno;
        int metadataRollbackResult = -1;
#ifdef SFV_ENABLE_TEST_HOOKS
        if (g_testFailMetadataRollbackUnlink) {
            g_testFailMetadataRollbackUnlink = false;
            errno = EACCES;
        } else
#endif
        {
            metadataRollbackResult = ::unlink(metadataPath.c_str());
        }

        if (metadataRollbackResult == 0 || errno == ENOENT) {
            if (!syncDirectoryPath(parent)) {
                if (publishedPathMayRemain != nullptr) {
                    *publishedPathMayRemain = true;
                }
                std::cerr
                    << "Metadata rollback removed the published record, but "
                    << "the directory rollback could not be synchronized; "
                    << "the name may reappear after a crash.\n";
            }
        } else {
            const int rollbackError = errno;
            if (publishedPathMayRemain != nullptr) {
                *publishedPathMayRemain = true;
            }
            std::cerr
                << "ERROR: complete metadata was published at '"
                << metadataPath
                << "', but directory synchronization failed ("
                << std::strerror(syncError)
                << ") and rollback unlink failed ("
                << std::strerror(rollbackError)
                << "). The metadata record may remain visible.\n";
        }

        if (::unlink(temporaryPath.c_str()) != 0 && errno != ENOENT) {
            std::cerr
                << "Metadata cleanup warning: temporary alias remains at '"
                << temporaryPath << "'.\n";
        }
        errno = syncError;
        return false;
    }

    if (::unlink(temporaryPath.c_str()) != 0 && errno != ENOENT) {
        std::cerr
            << "Metadata warning: final record is durable, but temporary alias "
            << "cleanup failed at '" << temporaryPath << "'.\n";
    } else if (!syncDirectoryPath(parent)) {
        std::cerr
            << "Metadata warning: final record is durable, but temporary alias "
            << "removal could reappear after a crash.\n";
    }

    return true;
}


bool loadArchiveMetadata(
    const std::string& metadataPath,
    ArchiveMetadata& metadata)
{
    std::ifstream file(metadataPath);

    if (!file.is_open()) {
        std::cerr
            << "Metadata error: could not open file.\n";
        std::cerr
            << "Path: "
            << metadataPath
            << "\n";
        return false;
    }

    std::string originalSizeText;
    std::string compressedSizeText;
    std::string percentageText;
    std::string archivedAtText;
    std::string originalPermissionsText;
    bool hasOriginalPermissionsLine = false;

    metadata.originalPermissions = 0600;
    metadata.hasOriginalPermissions = false;

    if (!std::getline(file, metadata.archiveId)) {
        std::cerr << "Metadata error: archiveId missing.\n";
        return false;
    }

    if (!std::getline(file, metadata.originalName)) {
        std::cerr << "Metadata error: originalName missing.\n";
        return false;
    }

    if (!std::getline(file, metadata.originalPath)) {
        std::cerr << "Metadata error: originalPath missing.\n";
        return false;
    }

    if (!std::getline(file, metadata.archivePath)) {
        std::cerr << "Metadata error: archivePath missing.\n";
        return false;
    }

    if (!std::getline(file, originalSizeText)) {
        std::cerr << "Metadata error: originalSize missing.\n";
        return false;
    }

    if (!std::getline(file, compressedSizeText)) {
        std::cerr << "Metadata error: compressedSize missing.\n";
        return false;
    }

    if (!std::getline(file, percentageText)) {
        std::cerr << "Metadata error: percentage missing.\n";
        return false;
    }

    if (!std::getline(file, archivedAtText)) {
        std::cerr << "Metadata error: archivedAt missing.\n";
        return false;
    }

    std::uintmax_t parsedOriginalSize = 0;
    std::uintmax_t parsedCompressedSize = 0;
    if (!parseUnsignedSize(originalSizeText, parsedOriginalSize)) {
        std::cerr
            << "Metadata error: original size must contain ASCII digits only "
            << "and fit in uintmax_t.\n";
        return false;
    }
    if (!parseUnsignedSize(compressedSizeText, parsedCompressedSize)) {
        std::cerr
            << "Metadata error: compressed size must contain ASCII digits only "
            << "and fit in uintmax_t.\n";
        return false;
    }

    if (std::getline(file, originalPermissionsText)) {
        hasOriginalPermissionsLine = true;
    }
    else if (!file.eof()) {
        std::cerr << "Metadata error: could not read permissions.\n";
        return false;
    }

    auto cleanLine = [](std::string& value)
    {
        // Remove Windows-style carriage return if present.
        if (!value.empty() && value.back() == '\r') {
            value.pop_back();
        }

        // Remove spaces/tabs from the beginning.
        std::size_t start =
            value.find_first_not_of(" \t");

        if (start != std::string::npos) {
            value.erase(0, start);
        }

        // Remove spaces/tabs from the end.
        std::size_t end =
            value.find_last_not_of(" \t");

        if (end != std::string::npos) {
            value.erase(end + 1);
        }
    };

    cleanLine(metadata.archiveId);
    cleanLine(metadata.originalName);
    cleanLine(metadata.originalPath);
    cleanLine(metadata.archivePath);

    cleanLine(percentageText);
    cleanLine(archivedAtText);
    if (hasOriginalPermissionsLine) {
        cleanLine(originalPermissionsText);
    }

    if (metadata.archiveId.empty()) {
        std::cerr << "Metadata error: archiveId is empty.\n";
        return false;
    }

    if (metadata.originalName.empty()) {
        std::cerr << "Metadata error: originalName is empty.\n";
        return false;
    }

    if (metadata.originalPath.empty()) {
        std::cerr << "Metadata error: originalPath is empty.\n";
        return false;
    }

    if (metadata.archivePath.empty()) {
        std::cerr << "Metadata error: archivePath is empty.\n";
        return false;
    }

    try {

        std::size_t position = 0;

        metadata.originalSize = parsedOriginalSize;
        metadata.compressedSize = parsedCompressedSize;

        metadata.compressionPercentage =
            std::stod(
                percentageText,
                &position
            );

        if (position != percentageText.size()) {
            std::cerr
                << "Metadata error: invalid compression percentage.\n";
            return false;
        }

        position = 0;

        metadata.archivedAt =
            std::stoll(
                archivedAtText,
                &position
            );

        if (position != archivedAtText.size()) {
            std::cerr
                << "Metadata error: invalid archive timestamp.\n";
            return false;
        }

        if (hasOriginalPermissionsLine) {
            if (!isUnsignedDecimal(originalPermissionsText)) {
                std::cerr
                    << "Metadata error: invalid original permissions.\n";
                return false;
            }

            position = 0;
            const unsigned long permissions =
                std::stoul(originalPermissionsText, &position);

            if (position != originalPermissionsText.size() ||
                permissions > 0777UL) {
                std::cerr
                    << "Metadata error: original permissions are out of range.\n";
                return false;
            }

            metadata.originalPermissions =
                static_cast<std::uint32_t>(permissions);
            metadata.hasOriginalPermissions = true;
        }
    }
    catch (const std::exception& error) {

        std::cerr
            << "Metadata conversion error: "
            << error.what()
            << "\n";

        return false;
    }

    return true;
}

bool createArchive(
    const std::string& inputPath,
    const std::string& archiveRoot,
    ArchiveMetadata& metadata
)
{
    std::error_code inputError;
    if (!fs::is_regular_file(inputPath, inputError) || inputError) {
        return false;
    }

    metadata.originalPermissions = 0600;
    metadata.hasOriginalPermissions = false;

    struct stat inputStatus {};
    if (::stat(inputPath.c_str(), &inputStatus) != 0) {
        std::cerr << "Archive error: could not read original permissions.\n";
        return false;
    }

    // Preserve only ordinary rwx bits. Special bits are not restored.
    metadata.originalPermissions =
        static_cast<std::uint32_t>(inputStatus.st_mode & 0777);
    metadata.hasOriginalPermissions = true;

    std::string compressedFolder =
        archiveRoot + "/compressed";

    std::string metadataFolder =
        archiveRoot + "/metadata";

    std::error_code error;

    fs::create_directories(
        compressedFolder,
        error
    );

    if (error) {
        return false;
    }

    fs::create_directories(
        metadataFolder,
        error
    );

    if (error) {
        return false;
    }

    // Persist the archive's directory entries before publishing data inside
    // its compressed/metadata subdirectories.
    fs::path archiveRootPath(archiveRoot);
    fs::path archiveRootParent = archiveRootPath.parent_path();
    if (archiveRootParent.empty()) {
        archiveRootParent = ".";
    }

    if (!syncDirectoryPath(archiveRootParent) ||
        !syncDirectoryPath(archiveRootPath)) {
        std::cerr << "Archive error: could not sync archive directories.\n";
        return false;
    }

    std::string archiveId =
        makeArchiveId();

    std::string originalName =
        fileNameFromPath(inputPath);

    std::string compressedPath =
        compressedFolder +
        "/" +
        archiveId +
        "_" +
        originalName +
        ".gz";

    if (!compressFile(
            inputPath,
            compressedPath
        )) {
        // Do not remove this path here: publication may have failed because
        // a destination already existed. TemporaryOutputFile reports the
        // rare case where a complete published output survives rollback.
        return false;
    }

    if (!compressionProvidesBenefit(
            inputPath,
            compressedPath
        )) {

        fs::remove(
            compressedPath,
            error
        );

        return false;
    }

    std::uintmax_t originalSize =
        fs::file_size(inputPath, error);
    if (error) {
        std::cerr << "Archive error: could not read original size.\n";
        error.clear();
        fs::remove(compressedPath, error);
        return false;
    }

    std::uintmax_t compressedSize =
        fs::file_size(compressedPath, error);
    if (error) {
        std::cerr << "Archive error: could not read compressed size.\n";
        error.clear();
        fs::remove(compressedPath, error);
        return false;
    }

    // A race or unexpected compressor result must never underflow the
    // unsigned storage calculation or create metadata claiming savings.
    if (compressedSize >= originalSize) {
        error.clear();
        fs::remove(compressedPath, error);
        return false;
    }

    const double saved =
        static_cast<double>(originalSize - compressedSize);

    double percentage =
        0.0;

    if (originalSize != 0) {

        percentage =
            (saved /
             static_cast<double>(
                 originalSize
             )) * 100.0;
    }

    metadata.archiveId =
        archiveId;

    metadata.originalName =
        originalName;

    const fs::path absoluteOriginal =
        fs::absolute(inputPath, error);
    if (error) {
        std::cerr << "Archive error: could not resolve original path.\n";
        error.clear();
        fs::remove(compressedPath, error);
        return false;
    }

    const fs::path absoluteArchive =
        fs::absolute(compressedPath, error);
    if (error) {
        std::cerr << "Archive error: could not resolve archive path.\n";
        error.clear();
        fs::remove(compressedPath, error);
        return false;
    }

    metadata.originalPath = absoluteOriginal.string();
    metadata.archivePath = absoluteArchive.string();

    metadata.originalSize =
        originalSize;

    metadata.compressedSize =
        compressedSize;

    metadata.compressionPercentage =
        percentage;

    auto now =
        std::chrono::system_clock::now();

    metadata.archivedAt =
        std::chrono::duration_cast<
            std::chrono::seconds
        >(
            now.time_since_epoch()
        ).count();

    std::string metadataPath =
        metadataFolder +
        "/" +
        archiveId +
        ".meta";

    bool metadataMayRemain = false;
    if (!saveArchiveMetadata(
            metadataPath,
            metadata,
            &metadataMayRemain
        )) {
        if (metadataMayRemain) {
            // The metadata and archive are complete, but the directory
            // durability rollback failed. Retain both files and keep the
            // original source; do not let archive-service cleanup mistake
            // this state for artifacts that were never published.
            std::cerr
                << "Archive warning: metadata rollback failed; preserving "
                << "the published archive, metadata, and original source.\n";
        } else {
            error.clear();
            fs::remove(compressedPath, error);
            if (error) {
                std::cerr
                    << "Archive cleanup warning: compressed rollback failed: "
                    << error.message() << '\n';
            }
        }

        // Do not pass a possibly colliding metadata path to generic cleanup.
        metadata = ArchiveMetadata{};
        return false;
    }

    return true;
}


/*
 * Read all valid archive metadata files.
 */
std::vector<ArchiveMetadata> getAllArchives(
    const std::string& archiveRoot
)
{
    std::vector<ArchiveMetadata> archives;

    std::string metadataFolder =
        archiveRoot + "/metadata";

    std::error_code error;
    if (!fs::exists(metadataFolder, error) || error) {
        return archives;
    }

    error.clear();
    if (!fs::is_directory(metadataFolder, error) || error) {
        return archives;
    }

    for (
        fs::directory_iterator it(
            metadataFolder,
            error
        );
        it != fs::directory_iterator();
        it.increment(error)
    ) {

        if (error) {
            break;
        }

        const fs::directory_entry& entry = *it;

        std::error_code entryError;

        if (!entry.is_regular_file(entryError)) {
            continue;
        }

        if (entry.path().extension() != ".meta") {
            continue;
        }

        ArchiveMetadata metadata;

        if (loadArchiveMetadata(
                entry.path().string(),
                metadata
            )) {

            archives.push_back(
                metadata
            );
        }
    }

    std::sort(
        archives.begin(),
        archives.end(),
        [](const ArchiveMetadata& a,
           const ArchiveMetadata& b) {

            return a.archivedAt >
                   b.archivedAt;
        }
    );

    return archives;
}


bool findArchiveById(
    const std::string& archiveRoot,
    const std::string& archiveId,
    ArchiveMetadata& metadata
)
{
    std::string metadataPath =
        archiveRoot +
        "/metadata/" +
        archiveId +
        ".meta";

    return loadArchiveMetadata(
        metadataPath,
        metadata
    );
}


static bool checkedArchiveSizeTotal(
    const std::vector<ArchiveMetadata>& archives,
    bool originalSize,
    std::uintmax_t& total
)
{
    const auto maximum =
        std::numeric_limits<std::uintmax_t>::max();

    total = 0;

    for (const ArchiveMetadata& archive : archives) {
        const std::uintmax_t size =
            originalSize ? archive.originalSize
                         : archive.compressedSize;

        if (size > maximum - total) {
            total = 0;
            return false;
        }

        total += size;
    }

    return true;
}

bool tryGetTotalOriginalSize(
    const std::string& archiveRoot,
    std::uintmax_t& total
)
{
    return checkedArchiveSizeTotal(
        getAllArchives(archiveRoot), true, total
    );
}

bool tryGetTotalCompressedSize(
    const std::string& archiveRoot,
    std::uintmax_t& total
)
{
    return checkedArchiveSizeTotal(
        getAllArchives(archiveRoot), false, total
    );
}

bool tryGetTotalSpaceSaved(
    const std::string& archiveRoot,
    std::uintmax_t& total
)
{
    total = 0;
    std::uintmax_t original = 0;
    std::uintmax_t compressed = 0;

    if (!tryGetTotalOriginalSize(archiveRoot, original) ||
        !tryGetTotalCompressedSize(archiveRoot, compressed)) {
        return false;
    }

    if (original > compressed) {
        total = original - compressed;
    }

    return true;
}

// Preserve the existing API. Saturate size totals on overflow;
// callers requiring explicit error reporting should use tryGet... .

std::uintmax_t getTotalOriginalSize(
    const std::string& archiveRoot
)
{
    std::uintmax_t total = 0;
    if (!tryGetTotalOriginalSize(archiveRoot, total)) {
        return std::numeric_limits<std::uintmax_t>::max();
    }
    return total;
}

std::uintmax_t getTotalCompressedSize(
    const std::string& archiveRoot
)
{
    std::uintmax_t total = 0;
    if (!tryGetTotalCompressedSize(archiveRoot, total)) {
        return std::numeric_limits<std::uintmax_t>::max();
    }
    return total;
}

std::uintmax_t getTotalSpaceSaved(
    const std::string& archiveRoot
)
{
    std::uintmax_t total = 0;
    if (!tryGetTotalSpaceSaved(archiveRoot, total)) {
        return 0;
    }
    return total;
}

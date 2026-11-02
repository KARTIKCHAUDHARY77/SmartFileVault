#include "archive.h"
#include "compression.h"

#include <filesystem>
#include <cctype>
#include <cmath>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

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

static bool syncFilePath(const fs::path& path)
{
    const int descriptor = ::open(path.c_str(), O_WRONLY);
    if (descriptor < 0) {
        return false;
    }

    bool success = syncFileDescriptor(descriptor);
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

// Parse an unsigned metadata integer without accepting signs, whitespace, or overflow.
static bool parseUnsignedDecimal(
    const std::string& text,
    std::uintmax_t& parsedValue
)
{
    if (!isUnsignedDecimal(text)) {
        return false;
    }

    const std::uintmax_t maximum =
        std::numeric_limits<std::uintmax_t>::max();

    std::uintmax_t value = 0;
    for (unsigned char character : text) {
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
    const ArchiveMetadata& metadata
)
{
    std::ofstream file(
        metadataPath
    );

    if (!file) {
        return false;
    }

    file << metadata.archiveId << '\n';
    file << metadata.originalName << '\n';
    file << metadata.originalPath << '\n';
    file << metadata.archivePath << '\n';
    file << metadata.originalSize << '\n';
    file << metadata.compressedSize << '\n';
    file << metadata.compressionPercentage << '\n';
    file << metadata.archivedAt << '\n';
    if (metadata.hasOriginalPermissions) {
        file << metadata.originalPermissions << '\n';
    }

    file.flush();
    if (!file.good()) {
        return false;
    }

    file.close();
    if (file.fail()) {
        return false;
    }

    // The metadata must reach stable storage before archiveAndRemoveOriginal
    // is allowed to remove the only source copy.
    if (!syncFilePath(fs::path(metadataPath))) {
        std::cerr << "Metadata error: could not sync metadata file.\n";
        return false;
    }

    fs::path parent = fs::path(metadataPath).parent_path();
    if (parent.empty()) {
        parent = ".";
    }

    if (!syncDirectoryPath(parent)) {
        std::cerr << "Metadata error: could not sync metadata directory.\n";
        return false;
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

    // getline() retains the carriage return from Windows CRLF files.
    // Remove only that line-ending character from numeric fields.
    // Spaces, tabs, signs, and other characters remain invalid.
    auto stripCarriageReturn = [](std::string& value)
    {
        if (!value.empty() && value.back() == '\r') {
            value.pop_back();
        }
    };

    stripCarriageReturn(originalSizeText);
    stripCarriageReturn(compressedSizeText);
    stripCarriageReturn(percentageText);
    stripCarriageReturn(archivedAtText);

    if (hasOriginalPermissionsLine) {
        stripCarriageReturn(originalPermissionsText);
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

        std::uintmax_t parsedOriginalSize = 0;
        std::uintmax_t parsedCompressedSize = 0;
        std::uintmax_t parsedTimestamp = 0;
        std::uintmax_t parsedPermissions = 0600;

        if (!parseUnsignedDecimal(originalSizeText, parsedOriginalSize)) {
            std::cerr
                << "Metadata error: original size must contain ASCII digits only and fit in uintmax_t.\n";
            return false;
        }

        if (!parseUnsignedDecimal(compressedSizeText, parsedCompressedSize)) {
            std::cerr
                << "Metadata error: compressed size must contain ASCII digits only and fit in uintmax_t.\n";
            return false;
        }

        // Archive timestamps are non-negative Unix epoch values.
        if (!parseUnsignedDecimal(archivedAtText, parsedTimestamp) ||
            parsedTimestamp > static_cast<std::uintmax_t>(
                std::numeric_limits<long long>::max())) {
            std::cerr << "Metadata error: invalid archive timestamp.\n";
            return false;
        }

        // std::stod accepts leading whitespace, so reject it explicitly.
        if (percentageText.empty()) {
            std::cerr << "Metadata error: compression percentage is empty.\n";
            return false;
        }

        for (unsigned char character : percentageText) {
            if (std::isspace(character)) {
                std::cerr << "Metadata error: whitespace in compression percentage.\n";
                return false;
            }
        }

        std::size_t position = 0;
        const double parsedPercentage = std::stod(percentageText, &position);
        if (position != percentageText.size() ||
            !std::isfinite(parsedPercentage) ||
            parsedPercentage < 0.0 ||
            parsedPercentage > 100.0) {
            std::cerr
                << "Metadata error: compression percentage must be finite and between 0 and 100.\n";
            return false;
        }

        if (hasOriginalPermissionsLine) {
            if (!parseUnsignedDecimal(
                    originalPermissionsText,
                    parsedPermissions
                ) || parsedPermissions > 0777U) {
                std::cerr
                    << "Metadata error: original permissions are out of range.\n";
                return false;
            }
        }

        // Assign numeric fields only after all numeric values have passed validation.
        metadata.originalSize = parsedOriginalSize;
        metadata.compressedSize = parsedCompressedSize;
        metadata.compressionPercentage = parsedPercentage;
        metadata.archivedAt = static_cast<long long>(parsedTimestamp);

        if (hasOriginalPermissionsLine) {
            metadata.originalPermissions =
                static_cast<std::uint32_t>(parsedPermissions);
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
    if (!fs::exists(inputPath) ||
        !fs::is_regular_file(inputPath)) {

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
        // A publication-directory sync can fail after the destination link
        // was created. Remove that exact generated path before returning so
        // a failed createArchive call cannot leave an untracked archive.
        error.clear();
        fs::remove(compressedPath, error);
        if (error) {
            std::cerr
                << "Archive cleanup warning: could not remove failed output: "
                << error.message() << '\n';
        }

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
        fs::file_size(inputPath);

    std::uintmax_t compressedSize =
        fs::file_size(compressedPath);

    double saved =
        static_cast<double>(
            originalSize - compressedSize
        );

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

    metadata.originalPath =
        fs::absolute(inputPath).string();

    metadata.archivePath =
        fs::absolute(compressedPath).string();

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

    if (!saveArchiveMetadata(
            metadataPath,
            metadata
        )) {

        fs::remove(
            compressedPath,
            error
        );

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

    if (!fs::exists(metadataFolder) ||
        !fs::is_directory(metadataFolder)) {

        return archives;
    }

    std::error_code error;

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

#include "archive.h"
#include "compression.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <cctype>
#include <cmath>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>
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

// A SHA-256 digest must contain exactly 64 ASCII hexadecimal characters.
static bool isValidSha256Hex(const std::string& digest)
{
    if (digest.size() != 64) {
        return false;
    }

    for (unsigned char character : digest) {
        const bool isDigit =
            character >= '0' && character <= '9';
        const bool isLowerHex =
            character >= 'a' && character <= 'f';
        const bool isUpperHex =
            character >= 'A' && character <= 'F';

        if (!isDigit && !isLowerHex && !isUpperHex) {
            return false;
        }
    }

    return true;
}

static bool isSignedDecimal(const std::string& value)
{
    if (value.empty()) {
        return false;
    }

    std::size_t index = value[0] == '-' ? 1U : 0U;
    if (index == value.size()) {
        return false;
    }

    for (; index < value.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(value[index]);
        if (character < '0' || character > '9') {
            return false;
        }
    }

    return true;
}

static bool parseSignedDecimal(const std::string& text, long long& value)
{
    if (!isSignedDecimal(text)) {
        return false;
    }

    try {
        std::size_t consumed = 0;
        const long long parsed = std::stoll(text, &consumed, 10);
        if (consumed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    }
    catch (...) {
        return false;
    }
}

static bool parseFileIdentityRecord(
    const std::string& line,
    OriginalFileIdentity& identity
)
{
    const std::string prefix = "FILEID:v1:";
    if (line.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }

    std::vector<std::string> fields;
    const std::string payload = line.substr(prefix.size());
    std::size_t start = 0;

    while (true) {
        const std::size_t separator = payload.find(':', start);
        if (separator == std::string::npos) {
            fields.push_back(payload.substr(start));
            break;
        }
        fields.push_back(payload.substr(start, separator - start));
        start = separator + 1;
    }

    if (fields.size() != 9) {
        return false;
    }

    OriginalFileIdentity parsed;
    std::uintmax_t modifiedNs = 0;
    std::uintmax_t changedNs = 0;
    std::uintmax_t birthNs = 0;

    if (!parseUnsignedDecimal(fields[0], parsed.device) ||
        !parseUnsignedDecimal(fields[1], parsed.inode) ||
        !parseUnsignedDecimal(fields[2], parsed.size) ||
        !parseSignedDecimal(fields[3], parsed.modifiedSeconds) ||
        !parseUnsignedDecimal(fields[4], modifiedNs) ||
        !parseSignedDecimal(fields[5], parsed.changedSeconds) ||
        !parseUnsignedDecimal(fields[6], changedNs) ||
        !parseSignedDecimal(fields[7], parsed.birthSeconds) ||
        !parseUnsignedDecimal(fields[8], birthNs) ||
        parsed.inode == 0 ||
        modifiedNs >= 1000000000ULL ||
        changedNs >= 1000000000ULL ||
        birthNs >= 1000000000ULL) {
        return false;
    }

    parsed.modifiedNanoseconds = static_cast<long long>(modifiedNs);
    parsed.changedNanoseconds = static_cast<long long>(changedNs);
    parsed.birthNanoseconds = static_cast<long long>(birthNs);
    identity = parsed;
    return true;
}

static bool sameIdentity(
    const OriginalFileIdentity& first,
    const OriginalFileIdentity& second
)
{
    return first.device == second.device &&
           first.inode == second.inode &&
           first.size == second.size &&
           first.modifiedSeconds == second.modifiedSeconds &&
           first.modifiedNanoseconds == second.modifiedNanoseconds &&
           first.changedSeconds == second.changedSeconds &&
           first.changedNanoseconds == second.changedNanoseconds &&
           first.birthSeconds == second.birthSeconds &&
           first.birthNanoseconds == second.birthNanoseconds;
}

bool isSafeArchiveId(const std::string& archiveId)
{
    if (archiveId.empty()) {
        return false;
    }

    bool containsAlphaNumeric = false;
    for (unsigned char character : archiveId) {
        const bool isUppercase = character >= 'A' && character <= 'Z';
        const bool isLowercase = character >= 'a' && character <= 'z';
        const bool isDigit = character >= '0' && character <= '9';

        if (!isUppercase && !isLowercase && !isDigit && character != '-') {
            return false;
        }
        containsAlphaNumeric = containsAlphaNumeric ||
                               isUppercase || isLowercase || isDigit;
    }

    return containsAlphaNumeric;
}

static bool identityFromStat(
    const struct stat& status,
    OriginalFileIdentity& identity
)
{
    if (!S_ISREG(status.st_mode) || status.st_size < 0) {
        return false;
    }

    identity.device = static_cast<std::uintmax_t>(status.st_dev);
    identity.inode = static_cast<std::uintmax_t>(status.st_ino);
    identity.size = static_cast<std::uintmax_t>(status.st_size);

#if defined(__APPLE__)
    identity.modifiedSeconds = static_cast<long long>(status.st_mtimespec.tv_sec);
    identity.modifiedNanoseconds = static_cast<long long>(status.st_mtimespec.tv_nsec);
    identity.changedSeconds = static_cast<long long>(status.st_ctimespec.tv_sec);
    identity.changedNanoseconds = static_cast<long long>(status.st_ctimespec.tv_nsec);
    identity.birthSeconds = static_cast<long long>(status.st_birthtimespec.tv_sec);
    identity.birthNanoseconds = static_cast<long long>(status.st_birthtimespec.tv_nsec);
#else
    identity.modifiedSeconds = static_cast<long long>(status.st_mtim.tv_sec);
    identity.modifiedNanoseconds = static_cast<long long>(status.st_mtim.tv_nsec);
    identity.changedSeconds = static_cast<long long>(status.st_ctim.tv_sec);
    identity.changedNanoseconds = static_cast<long long>(status.st_ctim.tv_nsec);
    identity.birthSeconds = 0;
    identity.birthNanoseconds = 0;
#endif

    return identity.inode != 0 &&
           identity.modifiedNanoseconds >= 0 &&
           identity.modifiedNanoseconds < 1000000000LL &&
           identity.changedNanoseconds >= 0 &&
           identity.changedNanoseconds < 1000000000LL &&
           identity.birthNanoseconds >= 0 &&
           identity.birthNanoseconds < 1000000000LL;
}

bool captureOriginalFileIdentity(
    const std::string& filePath,
    OriginalFileIdentity& identity
)
{
    struct stat status {};
    if (::lstat(filePath.c_str(), &status) != 0) {
        return false;
    }

    return identityFromStat(status, identity);
}

bool matchesOriginalFileIdentity(
    const std::string& filePath,
    const OriginalFileIdentity& expected
)
{
    OriginalFileIdentity current;
    return captureOriginalFileIdentity(filePath, current) &&
           sameIdentity(current, expected);
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
    if (metadata.hasOriginalSha256 &&
        !isValidSha256Hex(metadata.originalSha256)) {
        std::cerr << "Metadata error: invalid SHA-256 digest.\n";
        return false;
    }

    if (metadata.hasOriginalFileIdentity &&
        (metadata.originalFileIdentity.inode == 0 ||
         metadata.originalFileIdentity.size != metadata.originalSize ||
         metadata.originalFileIdentity.modifiedNanoseconds < 0 ||
         metadata.originalFileIdentity.modifiedNanoseconds >= 1000000000LL ||
         metadata.originalFileIdentity.changedNanoseconds < 0 ||
         metadata.originalFileIdentity.changedNanoseconds >= 1000000000LL ||
         metadata.originalFileIdentity.birthNanoseconds < 0 ||
         metadata.originalFileIdentity.birthNanoseconds >= 1000000000LL)) {
        std::cerr << "Metadata error: invalid original file identity.\n";
        return false;
    }

    std::ofstream file(metadataPath);

    if (!file) {
        return false;
    }

    // The first eight metadata lines are the legacy, stable schema.
    file << metadata.archiveId << '\n';
    file << metadata.originalName << '\n';
    file << metadata.originalPath << '\n';
    file << metadata.archivePath << '\n';
    file << metadata.originalSize << '\n';
    file << metadata.compressedSize << '\n';
    file << metadata.compressionPercentage << '\n';
    file << metadata.archivedAt << '\n';

    // Keep the existing optional permissions field in its original position.
    if (metadata.hasOriginalPermissions) {
        file << metadata.originalPermissions << '\n';
    }

    // The digest is appended after permissions, or occupies line nine when
    // the permissions field is absent.
    if (metadata.hasOriginalSha256) {
        file << "SHA256:" << metadata.originalSha256 << '\n';
    }

    if (metadata.hasOriginalFileIdentity) {
        const OriginalFileIdentity& identity = metadata.originalFileIdentity;
        file << "FILEID:v1:"
             << identity.device << ':'
             << identity.inode << ':'
             << identity.size << ':'
             << identity.modifiedSeconds << ':'
             << identity.modifiedNanoseconds << ':'
             << identity.changedSeconds << ':'
             << identity.changedNanoseconds << ':'
             << identity.birthSeconds << ':'
             << identity.birthNanoseconds << '\n';
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
    std::vector<std::string> optionalLines;

    metadata.originalPermissions = 0600;
    metadata.hasOriginalPermissions = false;
    metadata.originalSha256.clear();
    metadata.hasOriginalSha256 = false;
    metadata.originalFileIdentity = OriginalFileIdentity{};
    metadata.hasOriginalFileIdentity = false;

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

    auto stripCarriageReturn = [](std::string& value)
    {
        if (!value.empty() && value.back() == '\r') {
            value.pop_back();
        }
    };

    cleanLine(metadata.archiveId);
    cleanLine(metadata.originalName);
    cleanLine(metadata.originalPath);
    cleanLine(metadata.archivePath);

    // Numeric fields are strict: strip only the CR from a CRLF line ending.
    // Whitespace, signs and other characters remain invalid.
    stripCarriageReturn(originalSizeText);
    stripCarriageReturn(compressedSizeText);
    stripCarriageReturn(percentageText);
    stripCarriageReturn(archivedAtText);

    // Read at most three optional trailing lines: permissions, SHA-256,
    // and the versioned filesystem identity. Eight-line legacy metadata
    // and the two existing optional records remain valid.
    std::string optionalLine;
    while (std::getline(file, optionalLine)) {
        stripCarriageReturn(optionalLine);
        optionalLines.push_back(optionalLine);

        if (optionalLines.size() > 3) {
            std::cerr << "Metadata error: unexpected trailing metadata lines.\n";
            return false;
        }
    }

    if (file.bad()) {
        std::cerr << "Metadata error: could not read trailing metadata.\n";
        return false;
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

    std::string originalPermissionsText;
    std::string digestText;
    std::string identityText;
    bool hasOriginalPermissionsLine = false;
    bool hasDigestLine = false;
    bool hasIdentityLine = false;

    const std::string digestPrefix = "SHA256:";
    const std::string identityPrefix = "FILEID:";
    int optionalPhase = 0;

    for (const std::string& line : optionalLines) {
        if (line.compare(0, digestPrefix.size(), digestPrefix) == 0) {
            if (optionalPhase > 1 || hasDigestLine) {
                std::cerr << "Metadata error: misplaced SHA-256 record.\n";
                return false;
            }
            digestText = line.substr(digestPrefix.size());
            hasDigestLine = true;
            optionalPhase = 2;
        }
        else if (line.compare(0, identityPrefix.size(), identityPrefix) == 0) {
            if (optionalPhase > 2 || hasIdentityLine) {
                std::cerr << "Metadata error: misplaced file identity record.\n";
                return false;
            }
            identityText = line;
            hasIdentityLine = true;
            optionalPhase = 3;
        }
        else {
            // A plain optional line is the legacy permissions value and must
            // precede all tagged metadata records.
            if (optionalPhase != 0 || hasOriginalPermissionsLine) {
                std::cerr << "Metadata error: invalid optional metadata order.\n";
                return false;
            }
            originalPermissionsText = line;
            hasOriginalPermissionsLine = true;
            optionalPhase = 1;
        }
    }

    if (hasDigestLine && !isValidSha256Hex(digestText)) {
        std::cerr
            << "Metadata error: SHA-256 digest must contain exactly 64 hexadecimal characters.\n";
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

        OriginalFileIdentity parsedIdentity;
        if (hasIdentityLine &&
            (!parseFileIdentityRecord(identityText, parsedIdentity) ||
             parsedIdentity.size != parsedOriginalSize)) {
            std::cerr << "Metadata error: invalid original file identity record.\n";
            return false;
        }

        // Assign fields only after all supported metadata values validate.
        metadata.originalSize = parsedOriginalSize;
        metadata.compressedSize = parsedCompressedSize;
        metadata.compressionPercentage = parsedPercentage;
        metadata.archivedAt = static_cast<long long>(parsedTimestamp);

        if (hasOriginalPermissionsLine) {
            metadata.originalPermissions =
                static_cast<std::uint32_t>(parsedPermissions);
            metadata.hasOriginalPermissions = true;
        }

        if (hasDigestLine) {
            metadata.originalSha256 = digestText;
            metadata.hasOriginalSha256 = true;
        }

        if (hasIdentityLine) {
            metadata.originalFileIdentity = parsedIdentity;
            metadata.hasOriginalFileIdentity = true;
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
    metadata = ArchiveMetadata{};

    OriginalFileIdentity originalIdentity;
    if (!captureOriginalFileIdentity(inputPath, originalIdentity)) {
        std::cerr << "Archive error: source must be a non-symlink regular file with readable identity.\n";
        return false;
    }

    struct stat inputStatus {};
    if (::lstat(inputPath.c_str(), &inputStatus) != 0 ||
        !S_ISREG(inputStatus.st_mode)) {
        std::cerr << "Archive error: could not safely read original permissions.\n";
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

    // Capture the digest that compression verified against the source bytes
    // before, during, and after compression. It is published only when the
    // compressed output itself is successfully published.
    std::string originalSha256;

    if (!compressFile(
            inputPath,
            compressedPath,
            &originalSha256
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

    OriginalFileIdentity identityAfterCompression;
    if (!captureOriginalFileIdentity(inputPath, identityAfterCompression) ||
        !sameIdentity(originalIdentity, identityAfterCompression)) {
        std::cerr << "Archive error: original file identity changed during compression.\n";
        error.clear();
        fs::remove(compressedPath, error);
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

    std::uintmax_t originalSize = originalIdentity.size;

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
        fs::absolute(inputPath).lexically_normal().string();

    metadata.archivePath =
        fs::absolute(compressedPath).string();

    metadata.originalSize =
        originalSize;

    metadata.compressedSize =
        compressedSize;

    metadata.compressionPercentage =
        percentage;

    metadata.originalSha256 = originalSha256;
    metadata.hasOriginalSha256 = true;
    metadata.originalFileIdentity = originalIdentity;
    metadata.hasOriginalFileIdentity = true;

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
            // Catalogue entries remain visible when only metadata survives a
            // partial restore cleanup, but storage totals must reflect bytes
            // actually present in the trusted compressed directory.
            metadata.compressedSize = 0;
            if (isSafeArchiveId(metadata.archiveId) &&
                !metadata.originalName.empty() &&
                metadata.originalName != "." &&
                metadata.originalName != ".." &&
                metadata.originalName.find('/') == std::string::npos &&
                metadata.originalName.find('\\') == std::string::npos &&
                metadata.originalName.find('\0') == std::string::npos) {
                std::error_code pathError;
                const fs::path root = fs::absolute(archiveRoot, pathError).lexically_normal();
                if (!pathError) {
                    const fs::path expectedPath = fs::absolute(
                        root / "compressed" /
                        (metadata.archiveId + "_" + metadata.originalName + ".gz"),
                        pathError
                    ).lexically_normal();
                    if (!pathError &&
                        fs::path(metadata.archivePath).is_absolute() &&
                        fs::path(metadata.archivePath).lexically_normal() == expectedPath) {
                        struct stat compressedStatus {};
                        if (::lstat(expectedPath.c_str(), &compressedStatus) == 0 &&
                            S_ISREG(compressedStatus.st_mode) &&
                            compressedStatus.st_size >= 0) {
                            metadata.compressedSize =
                                static_cast<std::uintmax_t>(compressedStatus.st_size);
                        }
                    }
                }
            }

            archives.push_back(metadata);
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
    if (!isSafeArchiveId(archiveId)) {
        return false;
    }

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

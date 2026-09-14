#include "archive.h"

#include "compression.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

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

    return file.good();
}

bool loadArchiveMetadata(
    const std::string& metadataPath,
    ArchiveMetadata& metadata
)
{
    std::ifstream file(
        metadataPath
    );

    if (!file) {
        return false;
    }

    std::string originalSize;
    std::string compressedSize;
    std::string percentage;
    std::string archivedAt;

    if (!std::getline(
            file,
            metadata.archiveId
        )) {
        return false;
    }

    if (!std::getline(
            file,
            metadata.originalName
        )) {
        return false;
    }

    if (!std::getline(
            file,
            metadata.originalPath
        )) {
        return false;
    }

    if (!std::getline(
            file,
            metadata.archivePath
        )) {
        return false;
    }

    if (!std::getline(
            file,
            originalSize
        )) {
        return false;
    }

    if (!std::getline(
            file,
            compressedSize
        )) {
        return false;
    }

    if (!std::getline(
            file,
            percentage
        )) {
        return false;
    }

    if (!std::getline(
            file,
            archivedAt
        )) {
        return false;
    }

    try {

        metadata.originalSize =
            std::stoull(originalSize);

        metadata.compressedSize =
            std::stoull(compressedSize);

        metadata.compressionPercentage =
            std::stod(percentage);

        metadata.archivedAt =
            std::stoll(archivedAt);
    }
    catch (...) {
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

    // compressFile also checks whether
    // the source changed during compression.
    if (!compressFile(
            inputPath,
            compressedPath
        )) {

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
        (saved /
         static_cast<double>(originalSize))
        * 100.0;

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
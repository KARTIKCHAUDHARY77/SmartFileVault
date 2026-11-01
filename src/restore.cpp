
#include "restore.h"

#include "compression.h"
#include "file_utils.h"
#include "restore_safety.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace fs = std::filesystem;

// Reject metadata filenames that could escape the selected destination.
static bool isSafeRestoreFilename(const std::string& originalName)
{
    if (originalName.empty() ||
        originalName == "." ||
        originalName == "..") {
        return false;
    }

    // Filenames must not contain path separators or embedded null bytes.
    if (originalName.find('/') != std::string::npos ||
        originalName.find('\\') != std::string::npos ||
        originalName.find('\0') != std::string::npos) {
        return false;
    }

    // Reject control characters in metadata-supplied filenames.
    for (unsigned char character : originalName) {
        if (character < 0x20 || character == 0x7F) {
            return false;
        }
    }

    const fs::path filename(originalName);

    // A restored name must be one relative filename, not a path.
    return !filename.is_absolute() &&
           !filename.has_root_name() &&
           !filename.has_root_directory() &&
           !filename.has_parent_path() &&
           filename.filename() == filename;
}

static fs::path getSafeFilePath(
    const fs::path& directory,
    const std::string& originalName)
{
    fs::path originalPath = directory / originalName;

    std::error_code error;
    if (!fs::exists(originalPath, error)) {
        return error ? fs::path{} : originalPath;
    }

    fs::path namePath(originalName);

    std::string stem = namePath.stem().string();
    std::string extension = namePath.extension().string();

    int counter = 1;

    while (true) {

        std::string newName =
            stem + " (" +
            std::to_string(counter) +
            ")" +
            extension;

        fs::path newPath =
            directory / newName;

        error.clear();
        if (!fs::exists(newPath, error)) {
            return error ? fs::path{} : newPath;
        }

        if (counter == std::numeric_limits<int>::max()) {
            return {};
        }
        counter++;
    }
}

static bool filesMatch(
    const std::string& firstPath,
    const std::string& secondPath)
{
    try {

        std::error_code error;
        if (!fs::exists(firstPath, error) || error) {
            return false;
        }

        error.clear();
        if (!fs::exists(secondPath, error) || error) {
            return false;
        }

        error.clear();
        const auto firstSize = fs::file_size(firstPath, error);
        if (error) {
            return false;
        }

        error.clear();
        const auto secondSize = fs::file_size(secondPath, error);
        if (error || firstSize != secondSize) {
            return false;
        }

        std::ifstream first(
            firstPath,
            std::ios::binary
        );

        std::ifstream second(
            secondPath,
            std::ios::binary
        );

        if (!first.is_open() ||
            !second.is_open()) {
            return false;
        }

        const std::size_t bufferSize = 8192;

        char firstBuffer[bufferSize];
        char secondBuffer[bufferSize];

        while (first && second) {

            first.read(
                firstBuffer,
                bufferSize
            );

            second.read(
                secondBuffer,
                bufferSize
            );

            std::streamsize firstRead =
                first.gcount();

            std::streamsize secondRead =
                second.gcount();

            if (firstRead != secondRead) {
                return false;
            }

            for (std::streamsize i = 0;
                 i < firstRead;
                 i++) {

                if (firstBuffer[i] !=
                    secondBuffer[i]) {
                    return false;
                }
            }
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

bool restoreArchive(
    const std::string& metadataPath,
    const std::string& destinationDirectory,
    std::string& restoredPath)
{
    restoredPath.clear();

    ArchiveMetadata metadata;

    if (!loadArchiveMetadata(
            metadataPath,
            metadata)) {

        std::cerr
            << "Restore error: could not load metadata.\n";

        return false;
    }

    // Validate the filename before it is used in any output path.
    if (!isSafeRestoreFilename(metadata.originalName)) {

        std::cerr
            << "Restore error: unsafe filename in archive metadata.\n";

        return false;
    }

    std::cout
        << "Restore: metadata loaded.\n";

    if (metadata.archivePath.empty()) {

        std::cerr
            << "Restore error: archive path is empty.\n";

        return false;
    }

    std::error_code filesystemError;
    if (!fs::is_regular_file(metadata.archivePath, filesystemError) ||
        filesystemError) {

        std::cerr
            << "Restore error: archive does not exist.\n";

        std::cerr
            << "Archive path: "
            << metadata.archivePath
            << "\n";

        return false;
    }

    fs::path destination;

    if (!destinationDirectory.empty()) {
        destination = destinationDirectory;
    }
    else {
        destination =
            fs::path(metadata.originalPath).parent_path();
    }

    if (destination.empty()) {

        std::cerr
            << "Restore error: destination is empty.\n";

        return false;
    }

    filesystemError.clear();
    if (!fs::exists(destination, filesystemError)) {
        if (filesystemError) {
            std::cerr
                << "Restore error: could not inspect destination: "
                << filesystemError.message() << '\n';
            return false;
        }

        fs::create_directories(destination, filesystemError);
        if (filesystemError) {
            std::cerr
                << "Restore error: could not create destination: "
                << filesystemError.message() << '\n';
            return false;
        }
    }

    filesystemError.clear();
    if (!fs::is_directory(destination, filesystemError) ||
        filesystemError) {
        std::cerr
            << "Restore error: destination is not an accessible directory.\n";
        return false;
    }

    RestoreStorageStatus storage =
        checkRestoreSpace(
            metadata,
            destination.string()
        );

    if (!storage.spaceCheckSucceeded) {
        std::cerr
            << "Restore error: could not determine available free space. "
            << "Check the destination path and permissions.\n";
        return false;
    }

    if (!storage.enoughSpace) {

        std::cerr
            << "Restore error: insufficient free space.\n";

        std::cerr
            << "Required: "
            << storage.requiredSpace
            << " bytes\n";

        std::cerr
            << "Available: "
            << storage.freeSpace
            << " bytes\n";

        return false;
    }

    fs::path outputPath =
        getSafeFilePath(
            destination,
            metadata.originalName
        );
    if (outputPath.empty()) {
        std::cerr
            << "Restore error: could not select a safe output path.\n";
        return false;
    }

    std::cout
        << "Restore destination: "
        << outputPath.string()
        << "\n";

    const unsigned int outputPermissions =
        metadata.hasOriginalPermissions
            ? metadata.originalPermissions
            : 0600U;

    if (!decompressFile(
            metadata.archivePath,
            outputPath.string(),
            metadata.originalSize,
            outputPermissions)) {

        std::cerr
            << "Restore error: decompression failed.\n";

        return false;
    }

    filesystemError.clear();
    if (!fs::is_regular_file(outputPath, filesystemError) ||
        filesystemError) {

        std::cerr
            << "Restore error: output file was not created.\n";

        return false;
    }

    // If the original still exists, compare the restored
    // file with it byte-by-byte.
    filesystemError.clear();
    const bool originalExists =
        fs::exists(metadata.originalPath, filesystemError);
    if (filesystemError) {
        std::cerr
            << "Restore error: could not inspect original path: "
            << filesystemError.message() << '\n';
        std::error_code cleanupError;
        fs::remove(outputPath, cleanupError);
        return false;
    }

    filesystemError.clear();
    const bool originalIsRegular =
        originalExists &&
        fs::is_regular_file(metadata.originalPath, filesystemError);
    if (filesystemError) {
        std::cerr
            << "Restore error: could not inspect original file type: "
            << filesystemError.message() << '\n';
        std::error_code cleanupError;
        fs::remove(outputPath, cleanupError);
        return false;
    }

    if (originalExists && originalIsRegular) {

        if (!filesMatch(
                metadata.originalPath,
                outputPath.string())) {

            std::cerr
                << "Restore error: restored file does not match original.\n";

            std::error_code cleanupError;
            fs::remove(outputPath, cleanupError);
            if (cleanupError) {
                std::cerr
                    << "Restore cleanup warning: "
                    << cleanupError.message() << '\n';
            }

            return false;
        }
    }
    else {

        // Original file is no longer present.
        // At minimum, verify the original recorded size.
        filesystemError.clear();
        const std::uintmax_t restoredSize =
            fs::file_size(outputPath, filesystemError);
        if (filesystemError || restoredSize != metadata.originalSize) {

            std::cerr
                << "Restore error: restored file size is incorrect.\n";

            std::error_code cleanupError;
            fs::remove(outputPath, cleanupError);
            if (cleanupError) {
                std::cerr
                    << "Restore cleanup warning: "
                    << cleanupError.message() << '\n';
            }

            return false;
        }
    }

    restoredPath =
        outputPath.string();

    std::cout
        << "Restore verification passed.\n";

    return true;
}

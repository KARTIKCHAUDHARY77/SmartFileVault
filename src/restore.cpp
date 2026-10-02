#include "restore.h"

#include "compression.h"
#include "file_utils.h"
#include "restore_safety.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static fs::path getSafeFilePath(
    const fs::path& directory,
    const std::string& originalName)
{
    fs::path originalPath = directory / originalName;

    if (!fs::exists(originalPath)) {
        return originalPath;
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

        if (!fs::exists(newPath)) {
            return newPath;
        }

        counter++;
    }
}

static bool filesMatch(
    const std::string& firstPath,
    const std::string& secondPath)
{
    try {

        if (!fs::exists(firstPath) ||
            !fs::exists(secondPath)) {
            return false;
        }

        if (fs::file_size(firstPath) !=
            fs::file_size(secondPath)) {
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

    std::cout
        << "Restore: metadata loaded.\n";

    if (metadata.archivePath.empty()) {

        std::cerr
            << "Restore error: archive path is empty.\n";

        return false;
    }

    if (!fs::exists(metadata.archivePath)) {

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

    try {

        if (!fs::exists(destination)) {

            fs::create_directories(destination);
        }

        if (!fs::is_directory(destination)) {

            std::cerr
                << "Restore error: destination is not a directory.\n";

            return false;
        }
    }
    catch (...) {

        std::cerr
            << "Restore error: could not access destination.\n";

        return false;
    }

    RestoreStorageStatus storage =
        checkRestoreSpace(
            metadata,
            destination.string()
        );

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

    std::cout
        << "Restore destination: "
        << outputPath.string()
        << "\n";

    if (!decompressFile(
            metadata.archivePath,
            outputPath.string())) {

        std::cerr
            << "Restore error: decompression failed.\n";

        return false;
    }

    if (!fs::exists(outputPath)) {

        std::cerr
            << "Restore error: output file was not created.\n";

        return false;
    }

    // If the original still exists, compare the restored
    // file with it byte-by-byte.
    if (fs::exists(metadata.originalPath) &&
        fs::is_regular_file(metadata.originalPath)) {

        if (!filesMatch(
                metadata.originalPath,
                outputPath.string())) {

            std::cerr
                << "Restore error: restored file does not match original.\n";

            fs::remove(outputPath);

            return false;
        }
    }
    else {

        // Original file is no longer present.
        // At minimum, verify the original recorded size.
        if (fs::file_size(outputPath) !=
            metadata.originalSize) {

            std::cerr
                << "Restore error: restored file size is incorrect.\n";

            fs::remove(outputPath);

            return false;
        }
    }

    restoredPath =
        outputPath.string();

    std::cout
        << "Restore verification passed.\n";

    return true;
}
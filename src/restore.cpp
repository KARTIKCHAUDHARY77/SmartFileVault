#include "restore.h"

#include "compression.h"
#include "restore_safety.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static std::string getSafeFileName(
    const fs::path& directory,
    const std::string& originalName
)
{
    fs::path originalPath =
        directory / originalName;

    if (!fs::exists(originalPath)) {
        return originalPath.string();
    }

    fs::path name(originalName);

    std::string stem =
        name.stem().string();

    std::string extension =
        name.extension().string();

    int number = 1;

    while (true) {

        std::string newName =
            stem + " (" +
            std::to_string(number) +
            ")" +
            extension;

        fs::path newPath =
            directory / newName;

        if (!fs::exists(newPath)) {
            return newPath.string();
        }

        number++;
    }
}

static bool filesMatch(
    const std::string& firstPath,
    const std::string& secondPath
)
{
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

    if (!first || !second) {
        return false;
    }

    char firstBuffer[8192];
    char secondBuffer[8192];

    while (first && second) {

        first.read(
            firstBuffer,
            sizeof(firstBuffer)
        );

        second.read(
            secondBuffer,
            sizeof(secondBuffer)
        );

        std::streamsize firstCount =
            first.gcount();

        std::streamsize secondCount =
            second.gcount();

        if (firstCount != secondCount) {
            return false;
        }

        for (
            std::streamsize i = 0;
            i < firstCount;
            i++
        ) {
            if (firstBuffer[i] !=
                secondBuffer[i]) {

                return false;
            }
        }
    }

    return true;
}

bool restoreArchive(
    const std::string& metadataPath,
    const std::string& destinationDirectory,
    std::string& restoredPath
)
{
    ArchiveMetadata metadata;

    if (!loadArchiveMetadata(
            metadataPath,
            metadata
        )) {

        return false;
    }

    if (!fs::exists(
            metadata.archivePath
        )) {

        return false;
    }

    fs::path destination;

    if (!destinationDirectory.empty()) {

        destination =
            fs::path(destinationDirectory);

    } else {

        fs::path originalPath =
            fs::path(metadata.originalPath);

        destination =
            originalPath.parent_path();
    }

    // The original folder may have been removed.
    // In that case the caller should provide a
    // different destination directory.
    if (!fs::exists(destination) ||
        !fs::is_directory(destination)) {

        return false;
    }

    RestoreStorageStatus storage =
        checkRestoreSpace(
            metadata,
            destination.string()
        );

    if (!storage.enoughSpace) {
        return false;
    }

    std::string outputPath =
        getSafeFileName(
            destination,
            metadata.originalName
        );

    if (!decompressFile(
            metadata.archivePath,
            outputPath
        )) {

        fs::remove(outputPath);
        return false;
    }

    if (!filesMatch(
            metadata.originalPath,
            outputPath
        )) {

        /*
         * The original path may no longer exist.
         * In that case we still have another way
         * to verify size, but exact comparison
         * is only possible when the source exists.
         */
        if (fs::exists(metadata.originalPath)) {

            fs::remove(outputPath);
            return false;
        }

        if (fs::file_size(outputPath) !=
            metadata.originalSize) {

            fs::remove(outputPath);
            return false;
        }
    }

    restoredPath = outputPath;

    return true;
}
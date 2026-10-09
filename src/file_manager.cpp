#include "file_manager.h"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <exception>

namespace fs = std::filesystem;


// Record a file or directory that could not be processed.
static void addSkippedItem(
    FileScanResult& result,
    const std::string& path,
    const std::string& reason)
{
    result.skippedFiles++;

    SkippedFile skipped;
    skipped.filePath = path;
    skipped.reason = reason;

    result.skippedDetails.push_back(skipped);
}


// Convert filesystem errors into readable messages.
static std::string getErrorMessage(
    const std::error_code& error)
{
    if (error == std::errc::permission_denied) {
        return "Permission denied";
    }

    return error.message();
}


// Scan the contents of one directory.
// If a child directory cannot be scanned, return to its
// parent so that other directories can still be processed.
static void scanFolder(
    const fs::path& folderPath,
    int inactiveDays,
    FileScanResult& result)
{
    std::error_code iteratorError;

    fs::directory_iterator iterator(
        folderPath,
        iteratorError
    );

    if (iteratorError) {
        addSkippedItem(
            result,
            folderPath.string(),
            getErrorMessage(iteratorError)
        );

        return;
    }

    fs::directory_iterator end;

    while (iterator != end) {
        fs::path itemPath;

        try {
            itemPath = iterator->path();

            std::error_code typeError;

            bool isSymlink =
                iterator->is_symlink(typeError);

            if (typeError) {
                addSkippedItem(
                    result,
                    itemPath.string(),
                    getErrorMessage(typeError)
                );
            }
            else if (!isSymlink) {
                bool isDirectory =
                    iterator->is_directory(typeError);

                if (typeError) {
                    addSkippedItem(
                        result,
                        itemPath.string(),
                        getErrorMessage(typeError)
                    );
                }
                else if (isDirectory) {
                    // Scan this folder independently.
                    // A problem here should not stop
                    // scanning the parent's other entries.
                    scanFolder(
                        itemPath,
                        inactiveDays,
                        result
                    );
                }
                else {
                    bool isRegularFile =
                        iterator->is_regular_file(typeError);

                    if (typeError) {
                        addSkippedItem(
                            result,
                            itemPath.string(),
                            getErrorMessage(typeError)
                        );
                    }
                    else if (isRegularFile) {
                        // Check whether the file can be opened
                        // for reading before collecting metadata.
                        std::ifstream readableFile(
                            itemPath,
                            std::ios::binary
                        );

                        if (!readableFile.is_open()) {
                            addSkippedItem(
                                result,
                                itemPath.string(),
                                "Permission denied or file is not readable"
                            );
                        }
                        else {
                            readableFile.close();

                            FileMetadata metadata;

                            if (getFileMetadata(
                                    itemPath.string(),
                                    inactiveDays,
                                    metadata)) {
                                result.files.push_back(
                                    metadata
                                );
                            }
                            else {
                                addSkippedItem(
                                    result,
                                    itemPath.string(),
                                    "Could not read file metadata"
                                );
                            }
                        }
                    }
                }
            }
            // Symbolic links are ignored to avoid following
            // links outside the chosen directory or creating
            // directory traversal loops.
        }
        catch (const std::exception& error) {
            addSkippedItem(
                result,
                itemPath.empty()
                    ? folderPath.string()
                    : itemPath.string(),
                error.what()
            );
        }
        catch (...) {
            addSkippedItem(
                result,
                itemPath.empty()
                    ? folderPath.string()
                    : itemPath.string(),
                "Unknown filesystem error"
            );
        }

        // Use the non-throwing increment overload.
        std::error_code incrementError;

        iterator.increment(incrementError);

        if (incrementError) {
            addSkippedItem(
                result,
                folderPath.string(),
                "Could not continue scanning directory: "
                    + getErrorMessage(incrementError)
            );

            // This folder may be incomplete, but its caller
            // can continue processing other sibling folders.
            break;
        }
    }
}


bool scanDirectory(
    const std::string& directoryPath,
    int inactiveDays,
    FileScanResult& result)
{
    result.files.clear();
    result.skippedFiles = 0;
    result.skippedDetails.clear();

    if (directoryPath.empty() || inactiveDays <= 0) {
        return false;
    }

    try {
        fs::path root(directoryPath);

        std::error_code error;

        if (!fs::exists(root, error) || error) {
            return false;
        }

        if (!fs::is_directory(root, error) || error) {
            return false;
        }

        scanFolder(
            root,
            inactiveDays,
            result
        );

        return true;
    }
    catch (...) {
        return false;
    }
}
#include "file_manager.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

static void recordSkipped(FileScanResult& result,
                          const std::string& path,
                          const std::string& reason)
{
    result.skippedDetails.push_back({path, reason});
    result.skippedFiles =
        static_cast<int>(result.skippedDetails.size());
}

bool scanDirectory(const std::string& directoryPath,
                   int inactiveDays,
                   FileScanResult& result)
{
    result.files.clear();
    result.skippedFiles = 0;
    result.skippedDetails.clear();

    if (directoryPath.empty() || inactiveDays <= 0) {
        return false;
    }

    std::error_code error;
    const fs::path root(directoryPath);

    if (!fs::exists(root, error) || error) {
        return false;
    }

    if (!fs::is_directory(root, error) || error) {
        return false;
    }

    try {
        fs::recursive_directory_iterator iterator(
            root, fs::directory_options::skip_permission_denied, error);
        const fs::recursive_directory_iterator end;

        if (error) {
            recordSkipped(result, root.string(), error.message());
            return true;
        }

        while (iterator != end) {
            const fs::path itemPath = iterator->path();

            try {
                std::error_code typeError;

                if (iterator->is_symlink(typeError)) {
                    // Intentionally ignore symbolic links.
                } else if (typeError) {
                    recordSkipped(result, itemPath.string(),
                                  typeError.message());
                } else if (iterator->is_regular_file(typeError)) {
                    if (typeError) {
                        recordSkipped(result, itemPath.string(),
                                      typeError.message());
                    } else {
                        FileMetadata metadata{};

                        if (getFileMetadata(itemPath.string(),
                                            inactiveDays, metadata)) {
                            result.files.push_back(metadata);
                        } else {
                            recordSkipped(
                                result, itemPath.string(),
                                "Could not read file metadata");
                        }
                    }
                } else if (typeError) {
                    recordSkipped(result, itemPath.string(),
                                  typeError.message());
                }
            } catch (const std::exception& exception) {
                recordSkipped(result, itemPath.string(), exception.what());
            } catch (...) {
                recordSkipped(result, itemPath.string(),
                              "Unknown filesystem error");
            }

            iterator.increment(error);

            if (error) {
                recordSkipped(result, root.string(),
                              "Could not continue scanning directory: " +
                                  error.message());
                break;
            }
        }

        return true;
    } catch (const std::exception& exception) {
        recordSkipped(result, root.string(), exception.what());
        return true;
    } catch (...) {
        recordSkipped(result, root.string(), "Unknown filesystem error");
        return true;
    }
}

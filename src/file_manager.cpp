#include "file_manager.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

static void addSkippedFile(
    FileScanResult& result,
    const std::string& filePath,
    const std::string& reason)
{
    result.skippedFiles++;

    SkippedFile skipped;

    skipped.filePath = filePath;
    skipped.reason = reason;

    result.skippedDetails.push_back(skipped);
}


bool scanDirectory(
    const std::string& directoryPath,
    int inactiveDays,
    FileScanResult& result)
{
    result.files.clear();
    result.skippedFiles = 0;
    result.skippedDetails.clear();

    try {

        std::error_code initialError;

        if (!fs::exists(
                directoryPath,
                initialError))
        {
            return false;
        }

        if (initialError) {
            return false;
        }

        if (!fs::is_directory(
                directoryPath,
                initialError))
        {
            return false;
        }

        if (initialError) {
            return false;
        }


        fs::recursive_directory_iterator iterator(
            directoryPath,
            fs::directory_options::skip_permission_denied
        );

        fs::recursive_directory_iterator end;


        while (iterator != end)
        {
            try
            {
                const fs::directory_entry& entry =
                    *iterator;

                std::error_code entryError;

                bool regularFile =
                    entry.is_regular_file(
                        entryError
                    );

                if (entryError)
                {
                    addSkippedFile(
                        result,
                        entry.path().string(),
                        entryError.message()
                    );

                    ++iterator;
                    continue;
                }

                if (!regularFile)
                {
                    ++iterator;
                    continue;
                }


                // Check whether the file can be accessed
                // before collecting its metadata.
                std::error_code statusError;

                fs::file_status status =
                    fs::status(
                        entry.path(),
                        statusError
                    );

                if (statusError)
                {
                    if (statusError ==
                        std::make_error_code(
                            std::errc::permission_denied))
                    {
                        addSkippedFile(
                            result,
                            entry.path().string(),
                            "Permission denied"
                        );
                    }
                    else
                    {
                        addSkippedFile(
                            result,
                            entry.path().string(),
                            statusError.message()
                        );
                    }

                    ++iterator;
                    continue;
                }


                FileMetadata metadata;

                if (getFileMetadata(
                        entry.path().string(),
                        inactiveDays,
                        metadata))
                {
                    result.files.push_back(
                        metadata
                    );
                }
                else
                {
                    addSkippedFile(
                        result,
                        entry.path().string(),
                        "Could not read file metadata"
                    );
                }
            }
            catch (const std::exception& error)
            {
                addSkippedFile(
                    result,
                    "Unknown file",
                    error.what()
                );
            }
            catch (...)
            {
                addSkippedFile(
                    result,
                    "Unknown file",
                    "Unknown filesystem error"
                );
            }

            ++iterator;
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}
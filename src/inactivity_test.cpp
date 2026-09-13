#include "inactivity.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main() {

    const std::string testFile =
        "old_test.txt";

    {
        std::ofstream file(testFile);

        file << "SmartFileVault inactivity test";
    }

    FileMetadata metadata;

    if (!getFileMetadata(
            testFile,
            metadata
        )) {

        std::cout
            << "Metadata collection failed.\n";

        return 1;
    }

    std::cout
        << "Name: "
        << metadata.fileName
        << '\n';

    std::cout
        << "Path: "
        << metadata.filePath
        << '\n';

    std::cout
        << "Extension: "
        << metadata.extension
        << '\n';

    std::cout
        << "Size: "
        << metadata.fileSize
        << " bytes\n";

    std::cout
        << "Inactive at 90 days: "
        << (
            isFileInactive(
                metadata,
                90
            )
                ? "YES"
                : "NO"
        )
        << '\n';

    fs::remove(testFile);

    return 0;
}
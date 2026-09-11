#include "compression.h"

#include <filesystem>
#include <fstream>
#include <zlib.h>

namespace fs = std::filesystem;

bool compressFile(
    const std::string& inputPath,
    const std::string& outputPath
) {
    std::ifstream input(
        inputPath,
        std::ios::binary
    );

    if (!input) {
        return false;
    }

    gzFile output =
        gzopen(outputPath.c_str(), "wb");

    if (!output) {
        return false;
    }

    char buffer[8192];

    while (input) {

        input.read(buffer, sizeof(buffer));

        std::streamsize bytesRead =
            input.gcount();

        if (bytesRead <= 0) {
            continue;
        }

        int written = gzwrite(
            output,
            buffer,
            static_cast<unsigned int>(bytesRead)
        );

        if (written != bytesRead) {
            gzclose(output);
            return false;
        }
    }

    int closeResult = gzclose(output);

    return closeResult == Z_OK;
}

bool decompressFile(
    const std::string& compressedPath,
    const std::string& outputPath
) {
    gzFile input =
        gzopen(compressedPath.c_str(), "rb");

    if (!input) {
        return false;
    }

    std::ofstream output(
        outputPath,
        std::ios::binary
    );

    if (!output) {
        gzclose(input);
        return false;
    }

    char buffer[8192];

    int bytesRead;

    while ((bytesRead =
                gzread(
                    input,
                    buffer,
                    sizeof(buffer)
                )) > 0) {

        output.write(
            buffer,
            bytesRead
        );

        if (!output) {
            gzclose(input);
            return false;
        }
    }

    int errorNumber = Z_OK;

    gzerror(input, &errorNumber);

    gzclose(input);

    return errorNumber == Z_OK ||
           errorNumber == Z_STREAM_END;
}

bool compressionProvidesBenefit(
    const std::string& originalPath,
    const std::string& compressedPath
) {
    if (!fs::exists(originalPath) ||
        !fs::exists(compressedPath)) {

        return false;
    }

    if (!fs::is_regular_file(originalPath) ||
        !fs::is_regular_file(compressedPath)) {

        return false;
    }

    return fs::file_size(compressedPath) <
           fs::file_size(originalPath);
}

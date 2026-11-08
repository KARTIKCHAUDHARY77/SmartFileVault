#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <cstdint>
#include <string>

bool compressFile(
    const std::string& inputPath,
    const std::string& outputPath,
    std::string* originalSha256 = nullptr
);

bool decompressFile(
    const std::string& compressedPath,
    const std::string& outputPath,
    std::uintmax_t expectedOutputBytes,
    unsigned int outputPermissions = 0600U,
    const std::string& expectedSha256 = {}
);

bool compressionProvidesBenefit(
    const std::string& originalPath,
    const std::string& compressedPath
);

double getCompressionPercentage(
    const std::string& originalPath,
    const std::string& compressedPath
);

#endif

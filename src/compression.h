#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <string>

bool compressFile(
    const std::string& inputPath,
    const std::string& outputPath
);

bool decompressFile(
    const std::string& compressedPath,
    const std::string& outputPath
);

bool compressionProvidesBenefit(
    const std::string& originalPath,
    const std::string& compressedPath
);

#endif
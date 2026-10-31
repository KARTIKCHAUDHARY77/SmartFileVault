#ifndef COMPRESSION_POLICY_H
#define COMPRESSION_POLICY_H

#include <string>

enum class CompressionDecision {
    Compress,
    SkipAlreadyCompressed,
    SkipInvalidFile
};

CompressionDecision getCompressionDecision(
    const std::string& filePath
);

bool isCompressionResistant(
    const std::string& filePath
);

std::string decisionToString(
    CompressionDecision decision
);

#endif
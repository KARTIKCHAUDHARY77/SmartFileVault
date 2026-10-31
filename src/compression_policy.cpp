#include "compression_policy.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

bool isCompressionResistant(const std::string&) {
    // Extension-based exclusions are retired. This legacy helper
    // remains for source compatibility; eligibility is no longer
    // determined by filename extension.
    return false;
}

CompressionDecision getCompressionDecision(const std::string& filePath) {
    std::error_code error;
    const fs::file_status status = fs::status(filePath, error);

    if (error || !fs::is_regular_file(status)) {
        return CompressionDecision::SkipInvalidFile;
    }

    // This means "eligible for a compression attempt", not a promise
    // that compression will reduce size. The archive pipeline must
    // verify the round trip and reject outputs without a real benefit.
    return CompressionDecision::Compress;
}

std::string decisionToString(CompressionDecision decision) {
    switch (decision) {
        case CompressionDecision::Compress:
            return "COMPRESS";
        case CompressionDecision::SkipAlreadyCompressed:
            return "SKIP_ALREADY_COMPRESSED";
        case CompressionDecision::SkipInvalidFile:
            return "SKIP_INVALID_FILE";
    }

    return "UNKNOWN";
}

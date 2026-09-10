#include "compression_policy.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

namespace {

std::string getExtension(const std::string& filePath) {

    std::string extension =
        fs::path(filePath).extension().string();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c)
            );
        }
    );

    return extension;
}

const std::unordered_set<std::string>
compressionResistantExtensions = {

    // Archive formats
    ".zip",
    ".rar",
    ".7z",
    ".gz",
    ".bz2",
    ".xz",
    ".zst",

    // Image formats
    ".jpg",
    ".jpeg",
    ".png",
    ".webp",
    ".gif",
    ".heic",
    ".heif",

    // Video formats
    ".mp4",
    ".mkv",
    ".mov",
    ".avi",
    ".webm",

    // Audio formats
    ".mp3",
    ".aac",
    ".m4a",
    ".ogg",
    ".flac"
};

}

bool isCompressionResistant(
    const std::string& filePath
) {
    std::string extension =
        getExtension(filePath);

    return compressionResistantExtensions.find(extension)
           != compressionResistantExtensions.end();
}

CompressionDecision getCompressionDecision(
    const std::string& filePath
) {
    if (!fs::exists(filePath) ||
        !fs::is_regular_file(filePath)) {

        return CompressionDecision::SkipInvalidFile;
    }

    if (isCompressionResistant(filePath)) {
        return CompressionDecision::SkipAlreadyCompressed;
    }

    return CompressionDecision::Compress;
}

std::string decisionToString(
    CompressionDecision decision
) {
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
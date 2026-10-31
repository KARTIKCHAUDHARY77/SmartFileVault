
#include "compression.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <openssl/evp.h>
#include <zlib.h>

namespace fs = std::filesystem;

#ifdef SFV_ENABLE_TEST_HOOK
static void (*compressionTestHook)() = nullptr;

void setCompressionTestHook(void (*hook)()) {
    compressionTestHook = hook;
}
#endif

struct FileState {
    std::uintmax_t size;
    fs::file_time_type modifiedTime;
};

using Sha256Hash = std::array<unsigned char, 32>;

static bool getFileState(
    const std::string& filePath,
    FileState& state
) {
    std::error_code error;

    if (!fs::is_regular_file(filePath, error) || error) {
        return false;
    }

    state.size = fs::file_size(filePath, error);

    if (error) {
        return false;
    }

    state.modifiedTime = fs::last_write_time(filePath, error);

    return !error;
}

static bool fileChanged(
    const FileState& before,
    const std::string& filePath
) {
    FileState after;

    if (!getFileState(filePath, after)) {
        return true;
    }

    return before.size != after.size ||
           before.modifiedTime != after.modifiedTime;
}

static bool hashFile(
    const std::string& filePath,
    Sha256Hash& hash
) {
    std::ifstream input(filePath, std::ios::binary);

    if (!input) {
        return false;
    }

    EVP_MD_CTX* context = EVP_MD_CTX_new();

    if (context == nullptr) {
        return false;
    }

    bool success =
        EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1;

    char buffer[8192];

    while (success) {
        input.read(buffer, sizeof(buffer));

        const std::streamsize bytesRead = input.gcount();

        if (bytesRead > 0) {
            if (EVP_DigestUpdate(
                    context,
                    buffer,
                    static_cast<std::size_t>(bytesRead)
                ) != 1) {
                success = false;
                break;
            }
        }

        if (input.bad()) {
            success = false;
            break;
        }

        if (input.eof()) {
            break;
        }

        if (!input) {
            success = false;
            break;
        }
    }

    unsigned int hashLength = 0;

    if (success) {
        success =
            EVP_DigestFinal_ex(
                context,
                hash.data(),
                &hashLength
            ) == 1 &&
            hashLength == hash.size();
    }

    EVP_MD_CTX_free(context);

    return success;
}

bool compressFile(
    const std::string& inputPath,
    const std::string& outputPath
) {
    std::error_code error;

    try {
        if (fs::absolute(inputPath).lexically_normal() ==
            fs::absolute(outputPath).lexically_normal()) {
            return false;
        }
    } catch (const fs::filesystem_error&) {
        return false;
    }

    FileState before;

    if (!getFileState(inputPath, before)) {
        return false;
    }

    Sha256Hash hashBefore;

    if (!hashFile(inputPath, hashBefore)) {
        return false;
    }

    std::ifstream input(inputPath, std::ios::binary);

    if (!input) {
        return false;
    }

    if (fs::exists(outputPath, error) || error) {
        return false;
    }

    EVP_MD_CTX* context = EVP_MD_CTX_new();

    if (context == nullptr) {
        return false;
    }

    bool success =
        EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1;

    gzFile output = nullptr;

    if (success) {
        output = gzopen(outputPath.c_str(), "wb");

        if (output == nullptr) {
            success = false;
        }
    }

    char buffer[8192];
    #ifdef SFV_ENABLE_TEST_HOOK
    if (compressionTestHook != nullptr) {
        compressionTestHook();
    }
    #endif

    while (success) {
        input.read(buffer, sizeof(buffer));

        const std::streamsize bytesRead = input.gcount();

        if (bytesRead > 0) {
            if (EVP_DigestUpdate(
                    context,
                    buffer,
                    static_cast<std::size_t>(bytesRead)
                ) != 1) {
                success = false;
                break;
            }

            const int written = gzwrite(
                output,
                buffer,
                static_cast<unsigned int>(bytesRead)
            );

            if (written != bytesRead) {
                success = false;
                break;
            }
        }

        if (input.bad()) {
            success = false;
            break;
        }

        if (input.eof()) {
            break;
        }

        if (!input) {
            success = false;
            break;
        }
    }

    Sha256Hash hashDuring;
    unsigned int hashLength = 0;

    if (success) {
        success =
            EVP_DigestFinal_ex(
                context,
                hashDuring.data(),
                &hashLength
            ) == 1 &&
            hashLength == hashDuring.size();
    }

    EVP_MD_CTX_free(context);

    if (output != nullptr) {
        const int closeResult = gzclose(output);

        if (closeResult != Z_OK) {
            success = false;
        }
    }

    Sha256Hash hashAfter;

    if (!hashFile(inputPath, hashAfter)) {
        success = false;
    }

    if (success &&
        (hashBefore != hashDuring ||
         hashDuring != hashAfter)) {
        success = false;
    }

    if (fileChanged(before, inputPath)) {
        success = false;
    }

    if (!success) {
        fs::remove(outputPath, error);
        return false;
    }

    return true;
}

bool decompressFile(
    const std::string& compressedPath,
    const std::string& outputPath
) {
    try {
        if (fs::absolute(compressedPath).lexically_normal() ==
            fs::absolute(outputPath).lexically_normal()) {
            return false;
        }
    } catch (const fs::filesystem_error&) {
        return false;
    }

    std::ifstream headerFile(
        compressedPath,
        std::ios::binary
    );

    if (!headerFile) {
        return false;
    }

    unsigned char header[2] = {};

    headerFile.read(
        reinterpret_cast<char*>(header),
        sizeof(header)
    );

    if (headerFile.gcount() != 2 ||
        header[0] != 0x1f ||
        header[1] != 0x8b) {
        return false;
    }

    headerFile.close();

    std::error_code error;

    if (fs::exists(outputPath, error) || error) {
        return false;
    }

    gzFile input = gzopen(compressedPath.c_str(), "rb");

    if (input == nullptr) {
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

    bool success = true;
    std::uint64_t totalBytesRead = 0;
    char buffer[8192];

    while (true) {
        const int bytesRead = gzread(
            input,
            buffer,
            sizeof(buffer)
        );

        if (bytesRead < 0) {
            success = false;
            break;
        }

        if (bytesRead == 0) {
            break;
        }

                output.write(buffer, bytesRead);

        if (!output) {
            success = false;
            break;
        }

        totalBytesRead += static_cast<std::uint64_t>(bytesRead);
    }

    int errorCode = Z_OK;
    gzerror(input, &errorCode);

    if (errorCode != Z_OK) {
        success = false;
    }

    const int closeResult = gzclose(input);

    if (closeResult != Z_OK) {
        success = false;
    }

        output.close();

    if (!output) {
        success = false;
    }

    // Reject trailing data and concatenated members: the final 4 bytes of a
    // single-member GZIP file are the original size modulo 2^32.
    if (success) {
        std::ifstream footerFile(compressedPath, std::ios::binary);
        unsigned char footer[4] = {};

        if (!footerFile) {
            success = false;
        } else {
            footerFile.seekg(-4, std::ios::end);
            footerFile.read(reinterpret_cast<char*>(footer), 4);

            if (!footerFile || footerFile.gcount() != 4) {
                success = false;
            } else {
                const std::uint32_t expected =
                    static_cast<std::uint32_t>(footer[0]) |
                    (static_cast<std::uint32_t>(footer[1]) << 8) |
                    (static_cast<std::uint32_t>(footer[2]) << 16) |
                    (static_cast<std::uint32_t>(footer[3]) << 24);

                if (expected !=
                    static_cast<std::uint32_t>(totalBytesRead &
                                               0xFFFFFFFFu)) {
                    success = false;
                }
            }
        }
    }

    if (!success) {
        fs::remove(outputPath, error);
        return false;
    }

    return true;
}

bool compressionProvidesBenefit(
    const std::string& originalPath,
    const std::string& compressedPath
) {
    std::error_code error;

    if (!fs::is_regular_file(originalPath, error) || error) {
        return false;
    }

    if (!fs::is_regular_file(compressedPath, error) || error) {
        return false;
    }

    const auto originalSize =
        fs::file_size(originalPath, error);

    if (error) {
        return false;
    }

    const auto compressedSize =
        fs::file_size(compressedPath, error);

    if (error) {
        return false;
    }

    return compressedSize < originalSize;
}

double getCompressionPercentage(
    const std::string& originalPath,
    const std::string& compressedPath
) {
    if (!compressionProvidesBenefit(
            originalPath,
            compressedPath
        )) {
        return 0.0;
    }

    const std::uintmax_t originalSize =
        fs::file_size(originalPath);

    const std::uintmax_t compressedSize =
        fs::file_size(compressedPath);

    const double saved =
        static_cast<double>(originalSize - compressedSize);

    return (
        saved / static_cast<double>(originalSize)
    ) * 100.0;
}

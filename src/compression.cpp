
#include "compression.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <system_error>
#include <vector>

#include <unistd.h>
#include <zlib.h>

namespace fs = std::filesystem;

// Build output in a unique temporary file beside the destination. The final
// hard-link publication is atomic and fails rather than replacing an existing
// file. Keeping the temporary file in the same directory ensures that the
// publication cannot cross filesystem boundaries.
class TemporaryOutputFile {
public:
    TemporaryOutputFile() = default;

    TemporaryOutputFile(const TemporaryOutputFile&) = delete;
    TemporaryOutputFile& operator=(const TemporaryOutputFile&) = delete;

    ~TemporaryOutputFile() {
        if (descriptor_ >= 0) {
            ::close(descriptor_);
        }

        if (!temporaryPath_.empty() &&
            ::unlink(temporaryPath_.c_str()) != 0 &&
            errno != ENOENT) {
            std::cerr
                << "Warning: could not remove temporary output '"
                << temporaryPath_ << "'.\n";
        }
    }

    bool create(const std::string& destinationPath) {
        fs::path parent = fs::path(destinationPath).parent_path();

        if (parent.empty()) {
            parent = ".";
        }

        std::string pattern =
            (parent / ".sfv-output-XXXXXX").string();

        std::vector<char> writablePattern(
            pattern.begin(),
            pattern.end()
        );
        writablePattern.push_back('\0');

        descriptor_ = ::mkstemp(writablePattern.data());

        if (descriptor_ < 0) {
            return false;
        }

        temporaryPath_ = writablePattern.data();
        destinationPath_ = destinationPath;
        return true;
    }

    int descriptor() const {
        return descriptor_;
    }

    // Call only after gzdopen() or fdopen() successfully takes ownership.
    void releaseDescriptor() {
        descriptor_ = -1;
    }

    bool publish() {
        if (temporaryPath_.empty() || destinationPath_.empty()) {
            return false;
        }

        // link() creates the destination name atomically and returns EEXIST
        // if a file or symlink appeared there after the operation began.
        if (::link(
                temporaryPath_.c_str(),
                destinationPath_.c_str()
            ) != 0) {
            return false;
        }

        // The destination is now complete. If temporary-name cleanup fails,
        // keep the valid destination and let the destructor retry cleanup.
        if (::unlink(temporaryPath_.c_str()) == 0) {
            temporaryPath_.clear();
        }

        return true;
    }

private:
    int descriptor_ = -1;
    std::string temporaryPath_;
    std::string destinationPath_;
};

struct FileState {
    std::uintmax_t size;
    fs::file_time_type modifiedTime;
};

static bool getFileState(
    const std::string& filePath,
    FileState& state
) {
    if (!fs::exists(filePath) ||
        !fs::is_regular_file(filePath)) {
        return false;
    }

    std::error_code error;

    state.size = fs::file_size(filePath, error);

    if (error) {
        return false;
    }

    state.modifiedTime =
        fs::last_write_time(filePath, error);

    if (error) {
        return false;
    }

    return true;
}

static bool fileChanged(
    const FileState& before,
    const std::string& filePath
) {
    FileState after;

    if (!getFileState(filePath, after)) {
        return true;
    }

    if (before.size != after.size) {
        return true;
    }

    if (before.modifiedTime != after.modifiedTime) {
        return true;
    }

    return false;
}

bool compressFile(
    const std::string& inputPath,
    const std::string& outputPath
) {
    FileState before;

    if (!getFileState(inputPath, before)) {
        return false;
    }

    std::ifstream input(
        inputPath,
        std::ios::binary
    );

    if (!input) {
        return false;
    }

    TemporaryOutputFile temporaryOutput;

    if (!temporaryOutput.create(outputPath)) {
        return false;
    }

    gzFile output = gzdopen(
        temporaryOutput.descriptor(),
        "wb"
    );

    if (output == nullptr) {
        return false;
    }

    temporaryOutput.releaseDescriptor();

    bool success = true;
    char buffer[8192];

    while (success) {
        input.read(
            buffer,
            sizeof(buffer)
        );

        const std::streamsize bytesRead =
            input.gcount();

        if (bytesRead > 0) {
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

    if (gzclose(output) != Z_OK) {
        success = false;
    }

    // The source file may have changed while compression was taking place.
    if (success && fileChanged(before, inputPath)) {
        success = false;
    }

    // Publish only a fully closed archive. The destination is never opened
    // with truncation, and an existing destination is never overwritten.
    return success && temporaryOutput.publish();
}

bool decompressFile(
    const std::string& compressedPath,
    const std::string& outputPath,
    std::uintmax_t expectedOutputBytes
) {
    // Read the GZIP stream directly so we can enforce an exact output bound
    // and reject trailing bytes or concatenated GZIP members.
    std::ifstream input(
        compressedPath,
        std::ios::binary
    );

    if (!input) {
        return false;
    }

    TemporaryOutputFile temporaryOutput;

    if (!temporaryOutput.create(outputPath)) {
        return false;
    }

    FILE* output = fdopen(
        temporaryOutput.descriptor(),
        "wb"
    );

    if (output == nullptr) {
        return false;
    }

    temporaryOutput.releaseDescriptor();

    z_stream stream{};

    if (inflateInit2(&stream, MAX_WBITS + 16) != Z_OK) {
        std::fclose(output);
        return false;
    }

    char inputBuffer[8192];
    char outputBuffer[8192];

    std::uintmax_t totalOutputBytes = 0;
    bool success = true;
    bool streamEnded = false;
    bool inputEnded = false;

    while (success && !streamEnded) {
        if (stream.avail_in == 0 && !inputEnded) {
            input.read(
                inputBuffer,
                sizeof(inputBuffer)
            );

            const std::streamsize inputBytes = input.gcount();

            if (input.bad() || inputBytes < 0) {
                success = false;
                break;
            }

            if (input.eof()) {
                inputEnded = true;
            }

            if (inputBytes == 0) {
                success = false;
                break;
            }

            stream.next_in =
                reinterpret_cast<Bytef*>(inputBuffer);

            stream.avail_in =
                static_cast<uInt>(inputBytes);
        }

        stream.next_out =
            reinterpret_cast<Bytef*>(outputBuffer);

        stream.avail_out =
            static_cast<uInt>(sizeof(outputBuffer));

        const uInt inputBeforeInflate = stream.avail_in;

        const int inflateResult =
            inflate(&stream, Z_NO_FLUSH);

        const std::size_t producedBytes =
            sizeof(outputBuffer) - stream.avail_out;

        // This subtraction is safe because totalOutputBytes is only
        // increased after checking it against the expected bound.
        if (producedBytes >
            expectedOutputBytes - totalOutputBytes) {
            success = false;
            break;
        }

        if (producedBytes > 0) {
            const std::size_t bytesWritten =
                std::fwrite(
                    outputBuffer,
                    1,
                    producedBytes,
                    output
                );

            if (bytesWritten != producedBytes) {
                success = false;
                break;
            }

            totalOutputBytes +=
                static_cast<std::uintmax_t>(producedBytes);
        }

        if (inflateResult == Z_STREAM_END) {
            streamEnded = true;

            // SmartFileVault writes one GZIP member per archive. Reject
            // any unread input in this buffer, any following bytes, and
            // therefore concatenated members or trailing garbage.
            if (stream.avail_in != 0) {
                success = false;
                break;
            }

            const int nextByte = input.peek();

            if (nextByte != std::char_traits<char>::eof() ||
                input.bad() ||
                !input.eof()) {
                success = false;
            }

            break;
        }

        if (inflateResult != Z_OK &&
            inflateResult != Z_BUF_ERROR) {
            // Includes invalid GZIP headers, invalid DEFLATE data,
            // checksum failures, and malformed trailers.
            success = false;
            break;
        }

        if (inputEnded &&
            stream.avail_in == 0 &&
            producedBytes == 0) {
            // No more compressed input is available and the GZIP end
            // marker/trailer was not reached.
            success = false;
            break;
        }

        if (inputBeforeInflate == stream.avail_in &&
            producedBytes == 0) {
            if (stream.avail_in == 0 && !inputEnded) {
                continue;
            }

            success = false;
            break;
        }
    }

    if (inflateEnd(&stream) != Z_OK) {
        success = false;
    }

    // The metadata size is an exact expectation, not just a hint.
    // Reject short output as well as output that exceeds the bound.
    if (!streamEnded || totalOutputBytes != expectedOutputBytes) {
        success = false;
    }

    if (std::fflush(output) != 0) {
        success = false;
    }

    if (std::fclose(output) != 0) {
        success = false;
    }

    // Publish only a complete, valid, size-matched single-member GZIP
    // stream. On every failure the RAII owner removes the temporary file.
    return success && temporaryOutput.publish();
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

double getCompressionPercentage(
    const std::string& originalPath,
    const std::string& compressedPath
) {
    if (!fs::exists(originalPath) ||
        !fs::exists(compressedPath)) {
        return 0.0;
    }

    std::uintmax_t originalSize =
        fs::file_size(originalPath);

    std::uintmax_t compressedSize =
        fs::file_size(compressedPath);

    if (originalSize == 0) {
        return 0.0;
    }

    // Calculate the difference without unsigned
    // underflow when the compressed file is larger.
    double savedBytes;

    if (originalSize >= compressedSize) {
        savedBytes = static_cast<double>(
            originalSize - compressedSize
        );
    } else {
        savedBytes = -static_cast<double>(
            compressedSize - originalSize
        );
    }

    return (savedBytes /
            static_cast<double>(originalSize))
           * 100.0;
}

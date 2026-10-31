
#include "compression.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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
    const std::string& outputPath
) {
    gzFile input = gzopen(
        compressedPath.c_str(),
        "rb"
    );

    if (input == nullptr) {
        return false;
    }

    TemporaryOutputFile temporaryOutput;

    if (!temporaryOutput.create(outputPath)) {
        gzclose(input);
        return false;
    }

    FILE* output = fdopen(
        temporaryOutput.descriptor(),
        "wb"
    );

    if (output == nullptr) {
        gzclose(input);
        return false;
    }

    temporaryOutput.releaseDescriptor();

    bool success = true;
    char buffer[8192];
    int bytesRead = 0;

    while ((bytesRead = gzread(
                input,
                buffer,
                sizeof(buffer)
            )) > 0) {

        const std::size_t bytesWritten = std::fwrite(
            buffer,
            1,
            static_cast<std::size_t>(bytesRead),
            output
        );

        if (bytesWritten != static_cast<std::size_t>(bytesRead)) {
            success = false;
            break;
        }
    }

    int errorCode = Z_OK;
    gzerror(input, &errorCode);

    if (bytesRead < 0 || errorCode != Z_OK) {
        success = false;
    }

    if (gzclose(input) != Z_OK) {
        success = false;
    }

    if (std::fflush(output) != 0) {
        success = false;
    }

    if (std::fclose(output) != 0) {
        success = false;
    }

    // The destination name appears only after decompression and all stream
    // operations complete successfully. Existing files are never replaced.
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

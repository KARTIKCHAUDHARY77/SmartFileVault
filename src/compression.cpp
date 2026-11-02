
#include "compression.h"

#include <cerrno>
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <openssl/evp.h>
#include <string>
#include <stdlib.h>
#include <system_error>
#include <vector>

#include <unistd.h>
#include <sys/stat.h>
#include <zlib.h>

namespace fs = std::filesystem;


#ifdef SFV_ENABLE_TEST_HOOK
static void (*compressionTestHook)() = nullptr;

void setCompressionTestHook(void (*hook)()) {
    compressionTestHook = hook;
}
#endif

using Sha256Hash = std::array<unsigned char, 32>;

static bool syncFileDescriptor(int descriptor)
{
#if defined(__APPLE__) && defined(F_FULLFSYNC)
    if (::fcntl(descriptor, F_FULLFSYNC) == 0) {
        return true;
    }

    // Some filesystems do not implement F_FULLFSYNC. Use fsync as the
    // portable fallback only for an unsupported-operation response.
    if (errno != EINVAL && errno != ENOTSUP && errno != ENOTTY) {
        return false;
    }
#endif

    return ::fsync(descriptor) == 0;
}

static bool syncDirectoryPath(const fs::path& directory)
{
    const int descriptor = ::open(directory.c_str(), O_RDONLY);
    if (descriptor < 0) {
        return false;
    }

    bool success = (::fsync(descriptor) == 0);
    int savedError = errno;

    if (::close(descriptor) != 0 && success) {
        success = false;
        savedError = errno;
    }

    if (!success) {
        errno = savedError;
    }

    return success;
}

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
        if (syncDescriptor_ >= 0) {
            ::close(syncDescriptor_);
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

        // Keep a duplicate descriptor so the completed stream can be closed
        // by zlib/stdio before its data is synchronized to stable storage.
        syncDescriptor_ = ::dup(descriptor_);
        if (syncDescriptor_ < 0) {
            return false;
        }

        const int descriptorFlags = ::fcntl(syncDescriptor_, F_GETFD);
        if (descriptorFlags < 0 ||
            ::fcntl(
                syncDescriptor_,
                F_SETFD,
                descriptorFlags | FD_CLOEXEC
            ) != 0) {
            return false;
        }

        return true;
    }

    int descriptor() const {
        return descriptor_;
    }

    bool syncContents() {
        if (syncDescriptor_ < 0) {
            return false;
        }

        bool success = syncFileDescriptor(syncDescriptor_);
        int savedError = errno;

        if (::close(syncDescriptor_) != 0 && success) {
            success = false;
            savedError = errno;
        }
        syncDescriptor_ = -1;

        if (!success) {
            errno = savedError;
            return false;
        }

        contentsSynchronized_ = true;
        return true;
    }

    // Call only after gzdopen() or fdopen() successfully takes ownership.
    void releaseDescriptor() {
        descriptor_ = -1;
    }

    bool publish() {
        if (temporaryPath_.empty() || destinationPath_.empty() ||
            !contentsSynchronized_) {
            errno = EINVAL;
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

        fs::path parent = fs::path(destinationPath_).parent_path();
        if (parent.empty()) {
            parent = ".";
        }

        // Persist the destination directory entry before reporting success.
        // If this fails, attempt to remove the published name; callers will
        // preserve the original source because publication reports failure.
        if (!syncDirectoryPath(parent)) {
            const int syncError = errno;
            if (::unlink(destinationPath_.c_str()) == 0) {
                (void)syncDirectoryPath(parent);
            }
            errno = syncError;
            return false;
        }

        // The destination is now durable. Persist temporary-name removal too;
        // a failure here can leave only a harmless stale .sfv-output-* alias.
        if (::unlink(temporaryPath_.c_str()) == 0 || errno == ENOENT) {
            temporaryPath_.clear();
            if (!syncDirectoryPath(parent)) {
                std::cerr
                    << "Warning: completed output is durable, but temporary "
                    << "name cleanup may reappear after a crash.\n";
            }
        } else {
            std::cerr
                << "Warning: completed output is durable, but temporary "
                << "name cleanup failed and will be retried.\n";
        }

        return true;
    }

private:
    int descriptor_ = -1;
    int syncDescriptor_ = -1;
    bool contentsSynchronized_ = false;
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

    if (before.size != after.size) {
        return true;
    }

    if (before.modifiedTime != after.modifiedTime) {
        return true;
    }

    return false;
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
            hashLength == static_cast<unsigned int>(hash.size());
    }

    EVP_MD_CTX_free(context);
    return success;
}

bool compressFile(
    const std::string& inputPath,
    const std::string& outputPath
) {
    // Prevent attempts to compress a file onto itself.
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

    // Hash the original source before compression begins.
    Sha256Hash hashBefore{};

    if (!hashFile(inputPath, hashBefore)) {
        return false;
    }

    std::ifstream input(inputPath, std::ios::binary);

    if (!input) {
        return false;
    }

    // Use a private temporary file beside the destination.
    TemporaryOutputFile temporaryOutput;

    if (!temporaryOutput.create(outputPath)) {
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
        output = gzdopen(
            temporaryOutput.descriptor(),
            "wb"
        );

        if (output == nullptr) {
            success = false;
        } else {
            temporaryOutput.releaseDescriptor();
        }
    }

#ifdef SFV_ENABLE_TEST_HOOK
    // The regression test changes the source after the initial hash
    // but before the source is read for compression.
    if (success && compressionTestHook != nullptr) {
        compressionTestHook();
    }
#endif

    char buffer[8192];

    while (success) {
        input.read(buffer, sizeof(buffer));

        const std::streamsize bytesRead = input.gcount();

        if (bytesRead > 0) {
            // Hash exactly the bytes being compressed.
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

            if (written != static_cast<int>(bytesRead)) {
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

    Sha256Hash hashDuring{};
    unsigned int hashLength = 0;

    if (success) {
        success =
            EVP_DigestFinal_ex(
                context,
                hashDuring.data(),
                &hashLength
            ) == 1 &&
            hashLength == static_cast<unsigned int>(hashDuring.size());
    }

    EVP_MD_CTX_free(context);

    // Finish and close GZIP before synchronizing the temporary file.
    if (output != nullptr && gzclose(output) != Z_OK) {
        success = false;
    }

    // Hash the source again after compression.
    Sha256Hash hashAfter{};

    if (!hashFile(inputPath, hashAfter)) {
        success = false;
    }

    // Source content must be identical before, during and after compression.
    if (success &&
        (hashBefore != hashDuring ||
         hashDuring != hashAfter)) {
        success = false;
    }

    // Keep the additional size and modification-time check.
    if (fileChanged(before, inputPath)) {
        success = false;
    }

    if (!success) {
        return false;
    }

    if (!temporaryOutput.syncContents()) {
        std::cerr << "Compression error: could not sync completed archive.\n";
        return false;
    }

    // Atomically publish without overwriting an existing destination.
    return temporaryOutput.publish();
}

bool decompressFile(
    const std::string& compressedPath,
    const std::string& outputPath,
    std::uintmax_t expectedOutputBytes,
    unsigned int outputPermissions
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

    // Apply intended restore permissions only after decompressed
    // contents have been flushed and before the temporary file is closed.
    if (success &&
        ::fchmod(
            ::fileno(output),
            static_cast<mode_t>(outputPermissions & 0777U)
        ) != 0) {
        success = false;
    }



    if (std::fclose(output) != 0) {
        success = false;
    }

    // Synchronize only after the output stream has closed successfully.
    if (success && !temporaryOutput.syncContents()) {
        std::cerr << "Decompression error: could not sync completed output.\n";
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
    std::error_code error;

    if (!fs::is_regular_file(originalPath, error) || error) {
        return false;
    }

    error.clear();

    if (!fs::is_regular_file(compressedPath, error) || error) {
        return false;
    }

    const std::uintmax_t originalSize =
        fs::file_size(originalPath, error);

    if (error) {
        return false;
    }

    const std::uintmax_t compressedSize =
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
    std::error_code error;

    if (!fs::is_regular_file(originalPath, error) || error) {
        return 0.0;
    }

    error.clear();

    if (!fs::is_regular_file(compressedPath, error) || error) {
        return 0.0;
    }

    const std::uintmax_t originalSize =
        fs::file_size(originalPath, error);

    if (error || originalSize == 0) {
        return 0.0;
    }

    const std::uintmax_t compressedSize =
        fs::file_size(compressedPath, error);

    if (error || compressedSize >= originalSize) {
        return 0.0;
    }

    const std::uintmax_t savedBytes =
        originalSize - compressedSize;

    return (
        static_cast<double>(savedBytes) /
        static_cast<double>(originalSize)
    ) * 100.0;
}

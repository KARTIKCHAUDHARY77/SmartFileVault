
#include "compression.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <system_error>
#include <vector>

#include <unistd.h>
#include <sys/stat.h>
#include <zlib.h>

namespace fs = std::filesystem;

#ifdef SFV_ENABLE_TEST_HOOKS
static std::string g_testPublishDestination;
static bool g_testFailDirectorySync = false;
static bool g_testFailRollbackUnlink = false;

void setCompressionPublishFailureInjection(
    const std::string& destinationPath,
    bool failDirectorySync,
    bool failRollbackUnlink)
{
    g_testPublishDestination = destinationPath;
    g_testFailDirectorySync = failDirectorySync;
    g_testFailRollbackUnlink = failRollbackUnlink;
}
#endif

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
        bool directorySynced = false;
#ifdef SFV_ENABLE_TEST_HOOKS
        const bool injectForDestination =
            g_testPublishDestination == destinationPath_;
        const bool failDirectorySync =
            injectForDestination && g_testFailDirectorySync;
        const bool failRollbackUnlink =
            injectForDestination && g_testFailRollbackUnlink;
        if (injectForDestination) {
            g_testFailDirectorySync = false;
            g_testFailRollbackUnlink = false;
            g_testPublishDestination.clear();
        }
        if (failDirectorySync) {
            errno = EIO;
        } else
#endif
        {
            directorySynced = syncDirectoryPath(parent);
        }

#ifdef SFV_ENABLE_TEST_HOOKS
        if (injectForDestination && !failDirectorySync) {
            directorySynced = true;
        }
#endif
        if (
#ifdef SFV_ENABLE_TEST_HOOKS
            failDirectorySync || 
#endif
            !directorySynced) {
            const int syncError = errno == 0 ? EIO : errno;
            int rollbackResult = -1;
#ifdef SFV_ENABLE_TEST_HOOKS
            if (failRollbackUnlink) {
                errno = EACCES;
            } else
#endif
            {
                rollbackResult = ::unlink(destinationPath_.c_str());
            }

            if (rollbackResult == 0 || errno == ENOENT) {
                if (!syncDirectoryPath(parent)) {
                    std::cerr
                        << "Output rollback removed '" << destinationPath_
                        << "', but syncing the rollback directory failed; "
                        << "the name may reappear after a crash.\n";
                }
            } else {
                const int rollbackError = errno;
                std::cerr
                    << "ERROR: output '" << destinationPath_
                    << "' was published, but directory sync failed ("
                    << std::strerror(syncError)
                    << ") and rollback unlink failed ("
                    << std::strerror(rollbackError)
                    << "). The complete destination may remain visible; "
                    << "the caller must preserve the source.\n";
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
    if (!fs::exists(filePath, error) || error) {
        return false;
    }

    error.clear();
    if (!fs::is_regular_file(filePath, error) || error) {
        return false;
    }

    error.clear();
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

    // Publish only a fully closed and synchronized archive. The destination
    // is never opened with truncation or allowed to overwrite an existing file.
    if (success && !temporaryOutput.syncContents()) {
        std::cerr << "Compression error: could not sync completed archive.\n";
        success = false;
    }

    return success && temporaryOutput.publish();
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
            std::cerr
                << "Decompression error: output exceeds the expected size limit of "
                << expectedOutputBytes << " bytes.\n";
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
    if (!streamEnded) {
        success = false;
    } else if (totalOutputBytes != expectedOutputBytes) {
        std::cerr
            << "Decompression error: output size mismatch (expected "
            << expectedOutputBytes << " bytes, produced "
            << totalOutputBytes << " bytes).\n";
        success = false;
    }

    if (std::fflush(output) != 0) {
        success = false;
    }

    // Apply the requested mode only after the bytes have been written.
    // The temporary file remains 0600 until successful decompression.
    if (success &&
        ::fchmod(
            ::fileno(output),
            static_cast<mode_t>(outputPermissions & 0777U)
        ) != 0) {
        std::cerr
            << "Decompression error: could not apply restored file permissions.\n";
        success = false;
    }

    if (std::fclose(output) != 0) {
        success = false;
    }

    if (success && !temporaryOutput.syncContents()) {
        std::cerr << "Decompression error: could not sync completed output.\n";
        success = false;
    }

    // Publish only a complete, valid, size-matched single-member GZIP
    // stream with the requested permissions and durable contents.
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

    error.clear();
    const std::uintmax_t originalSize =
        fs::file_size(originalPath, error);
    if (error) {
        return false;
    }

    error.clear();
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
    if (!fs::exists(originalPath, error) || error) {
        return 0.0;
    }

    error.clear();
    if (!fs::exists(compressedPath, error) || error) {
        return 0.0;
    }

    error.clear();
    const std::uintmax_t originalSize =
        fs::file_size(originalPath, error);
    if (error || originalSize == 0) {
        return 0.0;
    }

    error.clear();
    const std::uintmax_t compressedSize =
        fs::file_size(compressedPath, error);
    if (error || compressedSize >= originalSize) {
        // Expansion is not a storage saving; the public contract is 0%.
        return 0.0;
    }

    const std::uintmax_t savedBytes = originalSize - compressedSize;
    return (static_cast<double>(savedBytes) /
            static_cast<double>(originalSize)) * 100.0;
}

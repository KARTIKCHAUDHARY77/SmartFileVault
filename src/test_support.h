#ifndef SMARTFILEVAULT_TEST_SUPPORT_H
#define SMARTFILEVAULT_TEST_SUPPORT_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>
#include <vector>

namespace testsupport {

namespace fs = std::filesystem;

// Creates a unique directory under the system temp folder.
// The directory is PRESERVED (and its path printed) unless cleanup()
// is called and succeeds, so failures and early returns keep evidence.
class TestDirectory {
public:
    explicit TestDirectory(const std::string& prefix) {
        std::error_code error;
        const fs::path base = fs::temp_directory_path(error);

        if (error) {
            return;
        }

        const auto stamp =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();

        for (int attempt = 0; attempt < 100; ++attempt) {
            const fs::path candidate =
                base / (prefix + "_" +
                        std::to_string(stamp) + "_" +
                        std::to_string(attempt));

            error.clear();

            // create_directory returns false (no error) if it
            // already exists, so we never reuse an old directory.
            if (fs::create_directory(candidate, error) && !error) {
                path_ = candidate;
                created_ = true;
                return;
            }

            if (error) {
                return;
            }
        }
    }

    TestDirectory(const TestDirectory&) = delete;
    TestDirectory& operator=(const TestDirectory&) = delete;

    ~TestDirectory() {
        if (created_ && !cleanedUp_) {
            std::cerr << "Test files preserved at: "
                      << path_.string() << '\n';
        }
    }

    bool ok() const { return created_; }
    const fs::path& path() const { return path_; }

    bool cleanup() {
        if (!created_) {
            return true;
        }

        std::error_code error;
        fs::remove_all(path_, error);

        if (error) {
            std::cerr << "FAIL: Test cleanup failed\n";
            return false;
        }

        cleanedUp_ = true;
        return true;
    }

private:
    fs::path path_;
    bool created_ = false;
    bool cleanedUp_ = false;
};

inline bool readAllBytes(const fs::path& path,
                         std::vector<unsigned char>& out) {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        return false;
    }

    out.assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );

    return !file.bad();
}

inline bool writeAllBytes(const fs::path& path,
                          const unsigned char* data,
                          std::size_t size) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    if (!file) {
        return false;
    }

    if (size > 0) {
        file.write(
            reinterpret_cast<const char*>(data),
            static_cast<std::streamsize>(size)
        );
    }

    file.flush();
    return static_cast<bool>(file);
}

inline bool writeAllBytes(const fs::path& path,
                          const std::vector<unsigned char>& bytes) {
    return writeAllBytes(path, bytes.data(), bytes.size());
}

// mt19937 output is fully specified by the C++ standard,
// so this is repeatable across standard libraries.
inline std::vector<unsigned char> pseudoRandomBytes(
    std::size_t count,
    std::uint32_t seed
) {
    std::mt19937 generator(seed);
    std::vector<unsigned char> bytes;
    bytes.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        bytes.push_back(
            static_cast<unsigned char>(generator() & 0xFFu)
        );
    }

    return bytes;
}

// Highly compressible, deterministic data.
inline std::vector<unsigned char> repeatedPatternBytes(
    std::size_t count
) {
    const std::string pattern = "SmartFileVault compressible data. ";
    std::vector<unsigned char> bytes;
    bytes.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        bytes.push_back(
            static_cast<unsigned char>(pattern[i % pattern.size()])
        );
    }

    return bytes;
}

} // namespace testsupport

#endif
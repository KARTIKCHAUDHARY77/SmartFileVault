# SmartFileVault

## Automated File Compression and Storage Management

SmartFileVault is a file management and storage optimization system designed to automatically identify inactive files and reduce unnecessary storage usage through compression and archiving.

The project combines a C/C++ based core system with a web-based frontend developed using HTML, CSS and JavaScript. The system is designed to scan files, analyze their activity, compress suitable inactive files, and provide options for managing and restoring archived files.

---

## Project Overview

Modern systems accumulate large numbers of files that are rarely accessed but continue to occupy storage space. Manually identifying and managing such files can be time-consuming.

SmartFileVault addresses this problem by providing an automated workflow for:

- Scanning files and directories
- Collecting file metadata
- Identifying inactive files
- Selecting files suitable for compression
- Compressing files to reduce storage usage
- Managing compressed archives
- Restoring files when required
- Providing a web-based interface for monitoring and control

---

## Objectives

The main objectives of SmartFileVault are:

- Automate identification of inactive files.
- Reduce unnecessary storage consumption.
- Provide reliable file compression and restoration.
- Maintain file metadata during storage management.
- Provide an easy-to-use web-based frontend.
- Create a modular system that can be extended in future phases.
- Evaluate compression effectiveness and system performance.

---

## System Workflow

```text
        File Input
            |
            v
      File Scanner
            |
            v
    Metadata Collection
            |
            v
    Inactivity Detection
            |
            v
   Compression Decision
            |
            v
    Compression Engine
            |
            v
   Compressed Storage
            |
            v
      Archive Management
            |
            v
       File Restoration

---

## Build Requirements

SmartFileVault requires:

- C++17 compatible compiler
- zlib for GZIP compression and decompression
- OpenSSL 3 for SHA-256 file integrity verification

### macOS Dependencies (Apple Silicon)

Install using Homebrew:

```bash
brew install zlib openssl@3
```

The commands below assume OpenSSL is at `/opt/homebrew/opt/openssl@3`.

### Compile and Run the Compression Tests

Test executables are written to `/tmp` so they do not end up in the repository.

```bash
OPENSSL=/opt/homebrew/opt/openssl@3

clang++ -std=c++17 -Wall -Wextra -pedantic \
-I"$OPENSSL/include" \
src/compression_test.cpp src/compression.cpp \
-L"$OPENSSL/lib" -lcrypto -lz \
-o /tmp/sfv_compression_test && /tmp/sfv_compression_test
```

Other compression tests use the same command with different sources:

| Test | Sources |
|------|---------|
| Compression policy | `src/compression_policy_test.cpp src/compression_policy.cpp` |
| Corruption handling | `src/compression_corruption_test.cpp src/compression.cpp` |
| Compression percentage | `src/compression_percentage_test.cpp src/compression.cpp` |
| Incompressible file | `src/incompressible_file_test.cpp src/archive.cpp src/compression.cpp` |
| Source change during compression | `src/compression_source_change_test.cpp src/compression.cpp` (add `-DSFV_ENABLE_TEST_HOOK`) |

`-DSFV_ENABLE_TEST_HOOK` enables a test-only hook in `compression.cpp`.
Never use it for a normal or production build.

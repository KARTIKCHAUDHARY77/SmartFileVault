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
## Build and Test (macOS)

Requirements: Clang with C++17 support, Python 3, and zlib.

### Build the API test server

Run from the repository root:

```bash
clang++ -std=c++17 -Wall -Wextra -pedantic \
  src/api_server_test.cpp \
  src/api_server.cpp \
  src/application_service.cpp \
  src/file_manager.cpp \
  src/inactivity.cpp \
  src/scan_report.cpp \
  src/vault_service.cpp \
  src/archive_service.cpp \
  src/archive.cpp \
  src/compression.cpp \
  src/compression_policy.cpp \
  src/file_utils.cpp \
  src/restore.cpp \
  src/restore_safety.cpp \
  src/vault_report.cpp \
  -lz -o /tmp/sfv_api_server
  ```

### Start the API server

Use a dedicated terminal and a temporary working directory:

```bash
API_RUN_DIR="$(mktemp -d /tmp/sfv-api-run.XXXXXX)"
cd "$API_RUN_DIR"
/tmp/sfv_api_server
```

Keep the server running while testing. Press `Ctrl+C` in that terminal to stop it.

### Run the API tests

In another terminal:

```bash
cd ~/Desktop/SmartFileVault

python3 tests/api_smoke_test.py \
  --base-url http://127.0.0.1:8080

python3 tests/api_request_parser_test.py
```

Only use the smoke test's optional scan mode with disposable test data and the required side-effect confirmation option.

### Run restore-space safety tests

```bash
cd ~/Desktop/SmartFileVault

clang++ -std=c++17 -Wall -Wextra -pedantic \
  src/restore_safety_test.cpp \
  src/restore_safety.cpp \
  src/archive.cpp \
  src/compression.cpp \
  src/compression_policy.cpp \
  src/file_utils.cpp \
  -lz -o /tmp/sfv_restore_safety_test

TEST_DIR="$(mktemp -d /tmp/sfv-safety.XXXXXX)"
(cd "$TEST_DIR" && /tmp/sfv_restore_safety_test)
```

### Run the end-to-end regression test

```bash
cd ~/Desktop/SmartFileVault

clang++ -std=c++17 -Wall -Wextra -pedantic \
  src/scan_restore_collision_test.cpp \
  src/application_service.cpp \
  src/file_manager.cpp \
  src/inactivity.cpp \
  src/scan_report.cpp \
  src/vault_service.cpp \
  src/archive_service.cpp \
  src/archive.cpp \
  src/compression.cpp \
  src/compression_policy.cpp \
  src/file_utils.cpp \
  src/restore.cpp \
  src/restore_safety.cpp \
  -lz -o /tmp/sfv_scan_restore_collision_test

/tmp/sfv_scan_restore_collision_test
```

### Final change checks

```bash
cd ~/Desktop/SmartFileVault
git diff --check
git status --short
git diff --stat
```

Review all changes before committing. Stage only explicitly reviewed files.
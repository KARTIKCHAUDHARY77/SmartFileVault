# SmartFileVault: Automated File Compression and Storage Optimization System
## Phase III Final Academic & Technical Project Report

**Academic Semester:** 3rd Semester B.Tech / Project Phase III  
**Repository:** `KARTIKCHAUDHARY77/SmartFileVault`  
**Latest Baseline Commit:** `6dc55eb`  
**Team Members & Roles:**
- **Kartik Chaudhary** (Lead Architecture, File Scanner, Storage & Concurrency Engineering, Release Management)
- **Akshat Garg** (Compression Hardening, Bounded Decompression, SHA-256 Verification, CI/CD, Safety Infrastructure)
- **Astha Negi** (Frontend UI/UX, REST API Client, Native Qt Desktop Application, Report Reconciliation)

---

## 1. Abstract

Modern filesystem usage patterns lead to the steady accumulation of dormant, rarely accessed files that unnecessarily deplete storage capacity. Conventional backup and compression solutions either require intrusive manual curation or operate as heavy background daemons without granular transparency or rollback guarantees.

**SmartFileVault** is a hybrid systems-level solution combining a high-performance **C++17 core engine**, a lightweight **REST API server**, a responsive **web frontend**, and a native **Qt 6 / QML desktop application**. SmartFileVault automates the discovery, inactivity classification, lossless compression (GZIP / Deflate), and verifiable archival of filesystem assets.

In Phase III, the system was hardened against real-world edge cases:
1. **Durable Archive/Restore Lifecycle**: Cryptographically verified SHA-256 integrity, bounded decompression limits (ZIP bomb defense), atomic temporary file publishing, and automatic archive cleanup.
2. **Permanent Deletion & Scan Reconciliation**: Dual-tier deletion protections ensuring uncorrupted original files are preserved unless explicitly verified, combined with atomic `scan_report.json` updates.
3. **Cross-Process Concurrency Protection**: Shared POSIX `flock` synchronization preventing race conditions between asynchronous directory scanning and deletion reconciliation.

---

## 2. System Architecture & Component Design

The SmartFileVault architecture is structured into decoupled modules with explicit ownership boundaries:

```
+-----------------------------------------------------------------------+
|                         Presentation Layer                            |
|   +----------------------------+     +----------------------------+   |
|   |   HTML5 / CSS3 / JS Web    |     |   Qt 6.12 / QML Native     |   |
|   |         Dashboard          |     |    Desktop Application     |   |
|   +--------------+-------------+     +--------------+-------------+   |
+------------------|----------------------------------|-----------------+
                   | HTTP / JSON                      | Direct C++ API
                   v                                  v
+-----------------------------------------------------------------------+
|                    Application & Service Layer                        |
|   +---------------------------------------------------------------+   |
|   |         ApplicationService (runScanAndArchive)                |   |
|   +---------------------------------------------------------------+   |
|   |         VaultService / ArchiveService / RestoreSafety         |   |
|   +---------------------------------------------------------------+   |
+------------------|----------------------------------|-----------------+
                   |                                  |
                   v                                  v
+-----------------------------------------------------------------------+
|                      Core Systems Engine (C++17)                      |
|  - Scanner & Metadata Collector (`file_manager.cpp`, `scanner.cpp`)   |
|  - Inactivity Classifier (`inactivity.cpp`)                           |
|  - Deflate/GZIP Engine & Policy (`compression.cpp`, `policy.cpp`)     |
|  - SHA-256 Verifier (`OpenSSL libcrypto`)                             |
|  - Concurrency Lock & Report Synchronizer (`scan_report.cpp`)         |
+-----------------------------------------------------------------------+
```

### 2.1 File Scanner & Metadata Engine
- Traverses target directory trees using C++17 `std::filesystem`.
- Extracts POSIX metadata (`stat`), calculating file size, extension, and last modified timestamp (`st_mtime`).
- Detects and isolates symlinks, skipping cyclic links and preserving external targets.
- Automatically excludes the active vault directory (`SmartFileVault_Vault` or configured root) to prevent recursive self-archival loops.

### 2.2 Inactivity Classifier & Compression Decision
- Files are marked inactive if:
  $$\Delta t = t_{\text{current}} - t_{\text{mtime}} \ge \text{threshold\_days} \times 86400$$
- Evaluates compression suitability based on file size (skipping 0-byte or trivial headers) and content compressibility without restricting extensions, ensuring maximum storage recovery across custom formats.

### 2.3 Compression & Archive Pipeline
- Utilizes `zlib` streaming deflate with strict error checking.
- Computes SHA-256 hashes concurrently with compression streams.
- Formats metadata sidecar files (`.meta`) containing original canonical paths, timestamps, sizes, permissions, and cryptographic digests.

### 2.4 Restore & Lifecycle Security
- Verifies target disk free space before commencing decompression (`statvfs`), preventing system disk exhaustion.
- Enforces strict decompression bounds: aborts immediately if output bytes exceed recorded metadata, mitigating zip-bomb vulnerabilities.
- Uses atomic file replacement (`mkstemp` + `rename`) so interrupted restores never leave partial corrupt destinations.

---

## 3. Phase III Concurrency & Data Integrity Engineering

### 3.1 The Concurrency Race Condition
During concurrent usage, two asynchronous workflows contend for the single source-of-truth report, `scan_report.json`:
1. **Scan & Publish**: Scans a directory of $N$ files and publishes the snapshot.
2. **Permanent Delete**: Destroys an archive, unlinks the original file, and reconciles `scan_report.json` to prune the deleted entry.

**The Failure Mode Without Synchronization:**
- Scan begins and catalogues `report.pdf`.
- Concurrently, a permanent delete finishes for `report.pdf` and purges it from `scan_report.json`.
- The scan completes and commits its pre-deletion snapshot, inadvertently resurrecting `report.pdf` in the dashboard despite its permanent deletion.

### 3.2 The Shared RAII POSIX Lock Architecture
To resolve this without introducing thread deadlocks with `VaultService::vaultOperationMutex`:
- Introduced `ScanReportLock` using POSIX `flock` on `<archiveRoot>/scan_report.json.lock`.
- The lock file is **never unlinked** across operations, guaranteeing all processes contend on the exact same filesystem inode.
- `runScanAndArchive()` acquires `ScanReportLock` *before* directory traversal starts and holds it continuously until the updated snapshot is flushed to disk via `saveScanReportLocked()`.
- The lock is released **prior** to calling `processInactiveFiles()`, preserving strict lock ordering and preventing lock-order inversion with the vault mutex.
- `reconcileScanReportAfterPermanentDelete()` acquires the identical `ScanReportLock` before reading `scan_report.json` and retains it through the atomic rename of the updated JSON report.

---

## 4. Verification & Testing Matrix

The repository incorporates automated regression suites executed on macOS:

| Test Target | Source File | Description | Status |
|---|---|---|---|
| **Concurrency Suite** | `tests/scan_report_concurrency_test.cpp` | Proves thread blocking and mutual exclusion between real `runScanAndArchive()` and `reconcileScanReportAfterPermanentDelete()` | **PASSED** |
| **Reconciliation Suite** | `tests/scan_report_reconciliation_test.cpp` | Validates exact-path matching, Unicode support, counter recalculation, and failure isolation | **PASSED** |
| **Permanent Deletion** | `src/archive_permanent_deletion_test.cpp` | Verifies identity protection: never unlinks original if content or inode differs | **PASSED** |
| **Compression Round-Trip** | `src/compression_test.cpp` | Byte-for-byte fidelity test across diverse data payloads | **PASSED** |
| **Bounded Decompression** | `src/decompression_limit_test.cpp` | Mitigates zip-bombs and truncated gzip streams | **PASSED** |
| **SHA-256 Cryptography** | `src/compression_sha256_test.cpp` | Verifies OpenSSL SHA-256 generation and mismatch rejection | **PASSED** |
| **REST API Server** | `tests/api_smoke_test.py` | Full HTTP lifecycle, CORS whitelist, and schema adherence | **PASSED** |
| **Frontend State Machine** | `tests/frontend_state_test.js` | UI state assertions, no-stale-marker verification | **PASSED** |
| **Desktop Native App** | `desktop/` via CMake + Qt 6 | Native GUI compilation, QML module linking, headless launch | **PASSED** |

---

## 5. Team Contribution Breakdown

| Team Member | Core Focus & Specific Deliverables |
|---|---|
| **Kartik Chaudhary** | - Project Lead & Core C++ Architecture<br>- High-performance directory scanning & metadata pipeline (`file_manager.cpp`)<br>- Concurrency architecture, `ScanReportLock` POSIX implementation, and test suite (`scan_report_concurrency_test.cpp`)<br>- Release orchestration, Git workflow, and technical report authoring |
| **Akshat Garg** | - Core compression algorithms & zlib streaming integration (`compression.cpp`)<br>- OpenSSL 3 SHA-256 digest calculation & metadata verification<br>- Bounded decompression engine & malicious archive rejection<br>- Continuous Integration suite (`run_macos_regression.sh`, GitHub Actions workflow) |
| **Astha Negi** | - Responsive Web UI frontend (HTML5/CSS3/ES6 JavaScript dashboard)<br>- Cross-platform Native Desktop application (`desktop/` using Qt 6.12 QML)<br>- Scan report reconciliation after permanent deletion (`scan_report.cpp`)<br>- Unicode filesystem path handling and test fixtures |

---

## 6. Conclusion & Future Scope

SmartFileVault Phase III successfully delivers an enterprise-grade, memory-safe, and concurrency-hardened storage optimization system. By combining POSIX systems primitives, modern C++17 RAII patterns, and dual user interfaces (Web + Qt 6 Desktop), the project meets all functional, performance, and safety objectives established in the project charter.

**Future Extensions:**
- Integration of modern dictionary-based algorithms (Zstandard / LZ4) for higher throughput.
- Cloud tiering (S3 / Google Cloud Storage) for offloading older vault archives.
- Background filesystem event watching (`fsevents` on macOS / `inotify` on Linux) to replace periodic polling with real-time dormancy detection.

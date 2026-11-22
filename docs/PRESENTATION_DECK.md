# SmartFileVault — Phase III Project Presentation Deck
## Automated File Compression & Storage Optimization System

**Presenters:** Kartik Chaudhary, Akshat Garg, Astha Negi  
**Course / Degree:** B.Tech Computer Science & Engineering (3rd Semester)  
**Project GitHub:** [KARTIKCHAUDHARY77/SmartFileVault](https://github.com/KARTIKCHAUDHARY77/SmartFileVault)

---

### Slide 1: Title & Overview
- **Header:** SmartFileVault: Phase III Final Presentation
- **Subheader:** An Automated, Safe, and Concurrent Storage Optimization System
- **Presenters:** Kartik Chaudhary, Akshat Garg, Astha Negi
- **Core Problem:** Digital bloat. Workstations accumulate gigabytes of inactive data, degrading I/O performance and exhausting SSD capacity without user awareness.
- **Our Solution:** A hybrid systems utility with automated inactivity detection, lossless compression, tamper-proof archival, dual graphical interfaces (Web & Native Qt), and POSIX concurrency guarantees.

---

### Slide 2: High-Level Architecture & Technology Stack
- **C++17 Core Engine:** High throughput, RAII memory management, zero-overhead systems access.
- **Storage & Crypto Layer:** `zlib` streaming deflate, OpenSSL 3 SHA-256 integrity verification.
- **Concurrency & IPC:** POSIX `flock` sidecar locks, atomic temp-file publishing (`mkstemp` + `rename`).
- **Interfaces:**
  - **Embedded Web Server:** Custom HTTP REST API server with loopback CORS binding.
  - **Modern Web Dashboard:** HTML5, CSS3 Grid, vanilla ES6 JavaScript.
  - **Native Desktop App:** Qt 6.12 / QML cross-platform application with asynchronous worker threads (`QtConcurrent`).

---

### Slide 3: The Lifecycle Pipeline: Scan -> Archive -> Restore
- **1. Directory Traversal:** Recursive scanning ignoring cyclic symlinks and excluding the vault itself.
- **2. Inactivity Metric:** Evaluates $\Delta t = t_{\text{now}} - t_{\text{mtime}} \ge \text{threshold}$.
- **3. Compression Decision:** Evaluates eligibility and lossless compressibility.
- **4. Vault Archival:** Encapsulates content into `<hash>.gz` alongside `<hash>.meta` (preserving permissions, timestamps, canonical source paths, and SHA-256).
- **5. Verified Restore:** Performs pre-flight disk capacity checks, bounds decompression to mitigate zip-bombs, verifies cryptographic checksums, and atomically places output.

---

### Slide 4: Phase III Concurrency Problem & Solution
- **The Challenge:** Scan operations and permanent deletion actions run asynchronously and both update `scan_report.json`.
- **The Race:** If a scan catalogues a file, and a concurrent deletion removes that file and cleans the report, an uncoordinated scan would overwrite the report with its stale pre-deletion snapshot—causing deleted files to "ghost" back into the UI.
- **Our Engineering Fix:**
  - Designed `ScanReportLock` using POSIX `flock` on `scan_report.json.lock`.
  - The lock file is never unlinked, ensuring all threads/processes bind to the identical inode.
  - Scan holds the lock from traversal through snapshot publication.
  - Lock is explicitly released *before* processing inactive files, avoiding lock inversion with the vault mutex.
  - Deletion reconciliation acquires the same lock across its atomic read-modify-write.

---

### Slide 5: Quality Assurance & Verification Results
- **Automated Regression Suite (`tests/run_macos_regression.sh`):**
  - Concurrency mutual-exclusion regression test: **PASSED**
  - Scan-report reconciliation & Unicode safety test: **PASSED**
  - Deletion identity preservation & symlink protection: **PASSED**
  - Lossless compression round-trip fidelity: **PASSED**
  - Bounded decompression & zip-bomb protection: **PASSED**
  - REST API lifecycle, CORS, and schema verification: **PASSED**
  - Frontend state machine consistency: **PASSED**
  - Native Qt 6 Desktop application build & headless launch: **PASSED**

---

### Slide 6: Team Roles & Contributions
- **Kartik Chaudhary (Architecture & Systems Lead):**
  - Designed core filesystem scanner and exclusion logic (`file_manager.cpp`).
  - Implemented POSIX `ScanReportLock`, non-recursive locked writers, and scan protection.
  - Authored deterministic multi-threaded concurrency regression suite (`scan_report_concurrency_test.cpp`).
- **Akshat Garg (Compression & Security Engineer):**
  - Implemented streaming zlib compression and round-trip verification (`compression.cpp`).
  - Integrated OpenSSL SHA-256 hashing and bounded decompression security limits.
  - Established continuous integration workflows and automated regression harness.
- **Astha Negi (Frontend & Application Engineer):**
  - Built responsive HTML5/CSS3/JavaScript web interface.
  - Implemented the native Qt 6 / QML desktop application and asynchronous controller (`dashboard_controller.cpp`).
  - Implemented scan report reconciliation logic for permanent deletion (`reconcileScanReportAfterPermanentDelete`).

---

### Slide 7: Live Demonstration Plan
- **Demo Part 1 (CLI & Automation):** Run the regression suite to showcase 100% automated test pass.
- **Demo Part 2 (Web Dashboard):** Launch via `scripts/run_smartfilevault.sh`, execute a scan, view inactive files, and perform a verified restore.
- **Demo Part 3 (Qt Desktop App):** Open `sfv_desktop`, configure inactivity threshold, trigger scanning, and demonstrate live archive statistics.

---

### Slide 8: Questions & Answers (Defense Preparation)

**Q1: Why use POSIX `flock` instead of a C++ `std::mutex`?**  
*Answer:* `std::mutex` only protects threads within a single process. Since the core utilities, test harnesses, web API server, and desktop application can run as separate OS processes, an OS-level file lock guarantees true inter-process mutual exclusion.

**Q2: Why not delete the `.lock` file when finished?**  
*Answer:* Deleting a lock file creates an inode race condition where two processes open different files with the same name before both lock, defeating mutual exclusion. Leaving the zero-byte lock file in place guarantees all processes flock the same inode.

**Q3: How do you prevent zip-bombs?**  
*Answer:* The `.meta` sidecar stores the uncompressed size. The decompression engine tracks decompressed bytes in real-time and aborts immediately if output exceeds the expected size by even 1 byte.

# SmartFileVault — Live Demonstration Script & Guide

This document outlines the step-by-step presentation script for conducting an end-to-end evaluation and live demonstration of SmartFileVault for faculty, evaluators, or peers.

---

## Pre-requisites & Setup (2 minutes before presentation)

Open a clean macOS Terminal window at the project repository:
```bash
cd ~/Desktop/SmartFileVault-Phase3-Recovery
```

Ensure OpenSSL 3 and Qt 6 are linked:
```bash
brew --prefix openssl@3
cmake --version
```

---

## Segment 1: Automated Verification & Security Proof (2 minutes)

**Speaker:** Akshat / Kartik

**Action:** Run the automated regression suite:
```bash
bash tests/run_macos_regression.sh
```

**Talking Points to Highlight While It Runs:**
1. **Targeted Coverage:** The suite compiles and executes compression round-trips, SHA-256 integrity verifications, bounded decompression, scanner reliability, API server lifecycle, and frontend state tests.
2. **Phase III Concurrency Fix:** Point out `scan_report_concurrency_test` output:
   - Proves `runScanAndArchive()` and `reconcileScanReportAfterPermanentDelete()` mutually exclude each other via `ScanReportLock`.
   - Demonstrates that a deleted file never gets resurrected by a concurrent scan snapshot.
3. Show the final banner: `========== MACOS REGRESSION SMOKE SUITE PASSED ==========`.

---

## Segment 2: Web Dashboard Live Demo (3 minutes)

**Speaker:** Astha / Kartik

**Action:** Prepare disposable test data and launch the full web application:
```bash
# 1. Create a disposable test folder with active and inactive files
DEMO_DIR="/tmp/sfv-demo-data"
rm -rf "$DEMO_DIR"
mkdir -p "$DEMO_DIR"

echo "Recent important report content" > "$DEMO_DIR/active_project.txt"
echo "Old log archive from last month" > "$DEMO_DIR/dormant_log.txt"

# Set dormant_log.txt modification time to 45 days in the past
touch -t 202610010000 "$DEMO_DIR/dormant_log.txt"

# 2. Launch the Web Application
bash scripts/run_smartfilevault.sh
```

**Talking Points & Actions in Web Browser (`http://127.0.0.1:8081`):**
1. **Dashboard Overview:** Point out the clean UI showing 0 archives, live storage capacity metrics, and inactive file detection settings.
2. **Execute Scan:**
   - In "Scan & Optimize", paste `/tmp/sfv-demo-data` as the target folder.
   - Set inactivity threshold to `30` days.
   - Click **Run Scan**.
3. **Inspect Results:**
   - `active_project.txt` is detected as **Active** (preserved in place).
   - `dormant_log.txt` is identified as **Inactive** (compressed and moved into the vault).
4. **Restore Workflow:**
   - Navigate to the **Restore** tab.
   - Select `dormant_log.txt` and click **Restore**.
   - Verify file is restored safely and the vault copy is cleaned up automatically.
5. Stop the web launcher with `Ctrl+C` in the terminal.

---

## Segment 3: Native Qt 6 Desktop Application Demo (3 minutes)

**Speaker:** Astha / Kartik

**Action:** Launch the native macOS desktop app:
```bash
./build/sfv_desktop.app/Contents/MacOS/sfv_desktop
```

**Talking Points & Actions in Native Window:**
1. **Native Integration:** Note the smooth native macOS styling, high-DPI rendering, and Qt 6 / QML modular components.
2. **Settings Tab:**
   - Demonstrate modifying the default inactivity threshold (e.g. 15 days).
   - Show the dynamic vault path resolution pointing to `SmartFileVault_Vault`.
3. **Files & Search:**
   - View scanned files and real-time status.
   - Show search filtering by filename and status (`active` vs `inactive`).
4. **Permanent Delete Both:**
   - Demonstrate the safe deletion capability: destroying an archive and securely unlinking the original while leaving unrelated files intact.

---

## Segment 4: Wrap-up & Q&A (2 minutes)

**Speaker:** Entire Team

**Closing Statement:**
> "SmartFileVault provides a complete, robust, and verified answer to workstation digital bloat—leveraging C++17 efficiency, POSIX concurrency guarantees, cryptographic data protection, and dual modern interfaces."

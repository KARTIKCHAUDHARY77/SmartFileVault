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

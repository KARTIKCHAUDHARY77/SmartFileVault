# SmartFileVault API Documentation

## 1. Overview

SmartFileVault exposes a local HTTP API for reading vault statistics,
listing archived files, retrieving scan reports, scanning directories,
and restoring archived files.

The API uses JSON request and response bodies.

## 2. Base URL

Default local development URL:

http://127.0.0.1:8080

The server must be running before clients can make requests.

The server is configured for local access. These endpoints are intended
for the SmartFileVault application running on the same computer.

## 3. Common Response Format

Responses use the following content type:

Content-Type: application/json

Error responses use this structure:

{
  "success": false,
  "message": "Description of the error."
}

The exact message depends on the endpoint and failure condition.

The server sends CORS headers allowing requests from browser clients.
This does not replace input validation or other security controls.

## 4. GET /api/dashboard

Returns vault statistics for the dashboard.

Example request:

curl -i http://127.0.0.1:8080/api/dashboard

The response includes the following fields:

| Field | Type | Description |
|---|---|---|
| archiveCount | integer | Number of archived records |
| originalSize | integer | Recorded original size in bytes |
| originalSizeText | string | Human-readable original size |
| compressedSize | integer | Recorded compressed size in bytes |
| compressedSizeText | string | Human-readable compressed size |
| spaceSaved | integer | Recorded space saved in bytes |
| spaceSavedText | string | Human-readable space saved |
| restoreSafetySpace | integer | Space required by the vault's restore-safety calculation |
| restoreSafetySpaceText | string | Human-readable restore-safety value |
| freeSpace | integer | Available filesystem space reported by the backend |
| freeSpaceText | string | Human-readable available space |
| archives | array | Archive metadata records |

The endpoint generates a vault report from the configured archive root.

Expected success status: 200 OK when the report can be generated
and read successfully.

Report-generation failures return a JSON error response.

## 5. GET /api/archives

Returns the vault report, including the archive list and associated
storage statistics.

Example request:

curl -i http://127.0.0.1:8080/api/archives

Each archive record contains fields such as:

| Field | Type | Description |
|---|---|---|
| archiveId | string | Identifier used to request restoration |
| originalName | string | Original filename |
| originalPath | string | Recorded original filesystem path |
| archivePath | string | Stored compressed archive path |
| originalSize | integer | Original size in bytes |
| originalSizeText | string | Human-readable original size |
| compressedSize | integer | Compressed size in bytes |
| compressedSizeText | string | Human-readable compressed size |
| compressionPercentage | number | Compression percentage reported by the backend |
| archivedAt | integer | Archive timestamp stored by the backend |

The actual archive data depends on the contents of the configured vault.

Expected success status: 200 OK when the vault report can be generated
and read successfully.

## 6. GET /api/files

Returns the most recently saved scan report.

Example request:

curl -i http://127.0.0.1:8080/api/files

The report contains:

| Field | Type | Description |
|---|---|---|
| totalFiles | integer | Number of files represented in the report |
| inactiveFiles | integer | Number of files classified as inactive |
| activeFiles | integer | Number of files classified as active |
| skippedFiles | integer | Number of files recorded as skipped |
| files | array | File metadata entries |

File records contain:

| Field | Type | Description |
|---|---|---|
| fileName | string | Filename |
| filePath | string | Filesystem path recorded during scanning |
| extension | string | File extension |
| size | integer | File size in bytes |
| sizeText | string | Human-readable file size |
| lastModified | integer | Modification timestamp reported by the scanner |
| status | string | Scan status, such as active or inactive |

The report describes a scan, rather than a live, continuously refreshed
view of the filesystem.

If no readable scan report is available, the current server returns
404 Not Found with a JSON error response.

## 7. POST /api/scan

Scans a folder and processes eligible inactive files.

Request headers:

Content-Type: application/json

Example request:

curl -i -X POST http://127.0.0.1:8080/api/scan \
  -H "Content-Type: application/json" \
  -d '{"folderPath":"/absolute/path/to/test-folder","inactiveDays":30}'

Request fields:

| Field | Type | Description |
|---|---|---|
| folderPath | string | Folder path accessible to the backend process |
| inactiveDays | integer | Positive inactivity threshold in days |

The current implementation rejects missing paths and non-positive
inactivity periods.

A successful response includes:

| Field | Type | Description |
|---|---|---|
| success | boolean | Whether the operation succeeded |
| scannedFiles | integer | Number of files scanned |
| scanSkippedFiles | integer | Files skipped during scanning |
| inactiveFiles | integer | Files identified as inactive |
| archivedFiles | integer | Files archived by the processing stage |
| skippedFiles | integer | Files skipped during processing |
| spaceSaved | integer | Space saved in bytes |

Expected status codes:

- 200 OK: scan and archive workflow reports success.
- 400 Bad Request: invalid scan parameters.
- 500 Internal Server Error: scan and archive operation failure.

This operation can modify the selected folder's contents. Eligible files
may be archived and their originals removed when archiving succeeds,
according to the backend implementation.

Use disposable test folders when testing this endpoint.

## 8. POST /api/restore

Requests restoration of an archived file to a destination directory.

Example request:

curl -i -X POST http://127.0.0.1:8080/api/restore \
  -H "Content-Type: application/json" \
  -d '{"archiveId":"ACTUAL_ARCHIVE_ID","destination":"/absolute/path/to/restore-folder"}'

Replace the example archive ID with an existing archive identifier and
use a suitable destination directory.

Request fields:

| Field | Type | Description |
|---|---|---|
| archiveId | string | Identifier of the archived file |
| destination | string | Destination directory for restoration |

A successful response contains:

{
  "success": true,
  "restoredPath": "/actual/path/to/restored-file"
}

The returned path identifies the restored file.

Expected status codes:

- 200 OK: restoration succeeds.
- 400 Bad Request: required parameters are missing or restoration fails.

The backend checks destination validity and available restore space.
The precise restoration outcome depends on the archive and filesystem
state. Test collision handling using disposable files before relying
on a specific overwrite or renaming behavior.

## 9. DELETE /api/archives/{archiveId}

Permanently removes one archive selected by its archive ID.

Example request:

```bash
curl -i -X DELETE \
  http://127.0.0.1:8080/api/archives/ACTUAL_ARCHIVE_ID
```

The archive ID is validated before any metadata path is constructed. The
backend derives the compressed and metadata paths from the configured vault
root and refuses unsafe or inconsistent paths.

The response reports each deletion step:

| Field | Type | Description |
|---|---|---|
| success | boolean | Whether the requested deletion completed without a partial failure |
| archiveId | string | Requested archive identifier |
| archiveDeleted | boolean | Whether the compressed artifact and metadata are absent |
| compressedRemoved | boolean | Whether the compressed artifact was removed or was already missing |
| metadataRemoved | boolean | Whether the metadata file was removed |
| originalDeleted | boolean | Whether the verified original file was removed |
| originalPreserved | boolean | Whether an original-path file was deliberately preserved |
| originalMissing | boolean | Whether the original file or its parent path was already missing |
| originalStatus | string | Reason the original was deleted, missing, or preserved |
| partialFailure | boolean | Whether some deletion steps completed before a later step failed |
| reportUpdated | boolean | Whether `vault_report.json` was refreshed immediately |
| errorCode | string | Machine-readable failure code, empty on success |
| message | string | Human-readable result or recovery information |

The backend only deletes the original when the versioned filesystem identity
stored in the metadata matches the file at the recorded original path. The
identity includes device, inode, size, nanosecond modification/change times,
and birth time where the platform exposes it. It does not follow a final
symlink. A replacement file, symlink, non-regular file, or legacy original
without identity metadata is preserved. A missing original or missing parent
directory does not prevent removal of the archived data. Legacy metadata remains
readable; it simply lacks the identity record needed to authorize original-file
deletion.

The compressed archive is removed first, followed by metadata. The original
is considered only after both archive artifacts are removed. If an artifact
cannot be removed, the operation reports its partial state and does not
proceed to delete the original. If report generation fails after deletion,
`reportUpdated` is false; subsequent dashboard/archive GET requests regenerate
the report from the catalogue.

Vault lifecycle operations invoked through `vault_service` are serialized
within the running process, including scan/archive processing, restore,
permanent deletion, and summary reads. The local HTTP server itself also
processes requests synchronously. This does not coordinate with a separate
SmartFileVault process or unrelated tools that mutate the filesystem.
The service rechecks the original's identity immediately before `unlink`, but
POSIX does not offer a portable atomic compare-and-unlink operation, so a
separate process with permission to rename entries in the original's parent
directory could still create a narrow time-of-check/time-of-use race.

The `FILEID:v1` record is an identity guard against accidental replacement and
stale paths, not a keyed signature or MAC. It does not authenticate metadata
against an attacker who can rewrite the vault metadata and fabricate identity
fields. The application assumes the vault metadata directory is writable only
by the trusted local user/application.

Expected status codes:

- 200 OK: archived data and metadata were removed. The original may have been
  deleted, already missing, or safely preserved with an explanatory status.
- 400 Bad Request: invalid archive ID, malformed metadata, an unsafe metadata
  file, or an unsafe recorded archive path.
- 404 Not Found: no metadata exists for the requested archive ID.
- 500 Internal Server Error: a deletion step or durability operation failed;
  inspect the response fields because a partial deletion may have occurred.

The frontend displays this exact confirmation before sending the request:

> This will permanently delete the archived file and its original file (if it exists). This action cannot be undone.

After a deletion response, the frontend refreshes Dashboard, Files/Archives,
and Restore views. The server's CORS method allow-list includes DELETE.

## 10. OPTIONS requests

The server supports OPTIONS requests for browser preflight handling.

Example:

curl -i -X OPTIONS http://127.0.0.1:8080/api/scan

The current implementation responds with HTTP 200 and an empty JSON
object.

## 11. Run API smoke tests

From the repository root, with the API server running:

python3 tests/api_smoke_test.py \
  --base-url http://127.0.0.1:8080

This runs the configured HTTP smoke checks without deliberately
starting a scan or restoring an archive.

The test script also permits an optional scan when explicitly requested.
Review the selected folder before allowing that operation.

## 12. Current limitations and testing notes

- The API server must be running before requests can succeed.
- A saved scan report may not exist until a scan has been performed.
- Filesystem paths must be accessible to the backend process.
- The report describes the saved scan, not necessarily the filesystem's
  current state.
- The API and its filesystem operations require local testing on the
  target operating system.
- The API smoke tests do not establish that every scan, compression,
  storage-limit, or restore edge case is covered.
- This document describes the current implementation; it does not
  promise behavior that has not been verified.

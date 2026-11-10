#!/usr/bin/env python3
"""End-to-end production API scan -> restore -> permanent-delete test.

All source files and vault data are created beneath the caller-supplied
disposable work directory. No user data or repository files are scanned.
"""

import argparse
import json
import os
import pathlib
import time
import urllib.error
import urllib.request


def request_json(base_url: str, path: str, method: str = "GET", payload=None):
    body = None if payload is None else json.dumps(payload).encode("utf-8")
    headers = {}
    if body is not None:
        headers["Content-Type"] = "application/json"
    request = urllib.request.Request(
        base_url + path,
        data=body,
        headers=headers,
        method=method,
    )
    try:
        with urllib.request.urlopen(request, timeout=15) as response:
            raw = response.read()
            return response.status, json.loads(raw.decode("utf-8"))
    except urllib.error.HTTPError as error:
        raw = error.read()
        try:
            response_body = json.loads(raw.decode("utf-8"))
        except Exception:
            response_body = {"raw": raw.decode("utf-8", errors="replace")}
        raise AssertionError(
            f"{method} {path} returned HTTP {error.code}: {response_body}"
        ) from error


def check(condition: bool, description: str) -> None:
    if not condition:
        raise AssertionError(description)
    print(f"PASS: {description}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-url", default="http://127.0.0.1:8080")
    parser.add_argument("--vault-root", required=True)
    parser.add_argument("--work-root", required=True)
    args = parser.parse_args()

    base_url = args.base_url.rstrip("/")
    vault_root = pathlib.Path(args.vault_root).resolve()
    work_root = pathlib.Path(args.work_root).resolve()
    source_root = work_root / "source"
    source_root.mkdir(parents=True, exist_ok=True)
    check(vault_root.is_dir(), "production vault directory exists")

    eligible = source_root / "eligible.txt"
    active = source_root / "recent-active.txt"
    expected = (b"SmartFileVault production lifecycle regression.\n" * 30000)
    active_bytes = b"Recent active fixture must remain unchanged.\n" * 1000
    eligible.write_bytes(expected)
    active.write_bytes(active_bytes)

    old_time = time.time() - (3 * 24 * 60 * 60)
    os.utime(eligible, (old_time, old_time))

    status, scan = request_json(
        base_url,
        "/api/scan",
        method="POST",
        payload={"folderPath": str(source_root), "inactiveDays": 1},
    )
    check(status == 200 and scan.get("success") is True,
          "production scan endpoint succeeds")
    check(scan.get("scannedFiles") == 2,
          "production scan discovers both disposable fixtures")
    check(scan.get("inactiveFiles") == 1 and scan.get("archivedFiles") == 1,
          "only the old eligible fixture is archived")
    check(not eligible.exists(),
          "verified archiving removes the eligible source file")
    check(active.is_file() and active.read_bytes() == active_bytes,
          "recent active source file remains byte-for-byte unchanged")

    status, catalogue = request_json(base_url, "/api/archives")
    archives = catalogue.get("archives")
    check(status == 200 and catalogue.get("archiveCount") == 1,
          "production archive catalogue contains one archive")
    check(isinstance(archives, list) and len(archives) == 1,
          "production archive catalogue returns the archive record")

    record = archives[0]
    archive_id = record.get("archiveId")
    check(bool(archive_id), "production archive record contains archiveId")
    check(record.get("originalName") == eligible.name,
          "production archive record identifies the eligible source")

    compressed_path = pathlib.Path(record["archivePath"])
    metadata_path = vault_root / "metadata" / f"{archive_id}.meta"
    check(compressed_path.is_file() and metadata_path.is_file(),
          "compressed archive and metadata are published in the production vault")

    status, restored = request_json(
        base_url,
        "/api/restore",
        method="POST",
        payload={"archiveId": archive_id, "destination": str(source_root)},
    )
    restored_path = pathlib.Path(restored.get("restoredPath", ""))
    check(status == 200 and restored.get("success") is True and
          restored.get("restored") is True,
          "production restore endpoint reports successful publication")
    check(restored.get("archiveDeleted") is True and
          restored.get("compressedRemoved") is True and
          restored.get("metadataRemoved") is True and
          restored.get("partialFailure") is False,
          "successful restore removes both vault artifacts")
    check(restored.get("reportUpdated") is True,
          "successful restore regenerates the vault report")
    check(restored_path == eligible and restored_path.is_file() and
          restored_path.read_bytes() == expected,
          "restored output is byte-for-byte identical at the original location")
    check(not compressed_path.exists() and not metadata_path.exists(),
          "restore removes compressed archive and metadata")

    status, after_restore = request_json(base_url, "/api/dashboard")
    check(status == 200 and after_restore.get("archiveCount") == 0 and
          after_restore.get("archives") == [] and after_restore.get("compressedSize") == 0,
          "dashboard and archive catalogue no longer include the restored archive")

    user_edit = b"\nUser edit made after restoration.\n"
    with restored_path.open("ab") as restored_file:
        restored_file.write(user_edit)
    expected_after_user_edit = expected + user_edit
    check(restored_path.read_bytes() == expected_after_user_edit,
          "restored file remains editable after automatic archive cleanup")

    # Exercise the existing Delete Both route against a separate disposable archive.
    delete_both = source_root / "delete-both.txt"
    delete_both_bytes = b"Independent Delete Both regression fixture.\n" * 2000
    delete_both.write_bytes(delete_both_bytes)
    old_time = time.time() - (3 * 24 * 60 * 60)
    os.utime(delete_both, (old_time, old_time))
    status, second_scan = request_json(
        base_url,
        "/api/scan",
        method="POST",
        payload={"folderPath": str(source_root), "inactiveDays": 1},
    )
    check(status == 200 and second_scan.get("archivedFiles") == 1,
          "separate disposable fixture archived for Delete Both regression")
    status, second_catalogue = request_json(base_url, "/api/archives")
    second_archives = second_catalogue.get("archives", [])
    check(status == 200 and len(second_archives) == 1,
          "separate Delete Both fixture appears in the vault")
    delete_id = second_archives[0]["archiveId"]
    delete_record = second_archives[0]
    delete_compressed = pathlib.Path(delete_record["archivePath"])
    delete_metadata = vault_root / "metadata" / f"{delete_id}.meta"
    check(delete_compressed.is_file() and delete_metadata.is_file(),
          "Delete Both fixture has both vault artifacts")

    status, deleted = request_json(
        base_url, f"/api/archives/{delete_id}", method="DELETE"
    )
    check(status == 200 and deleted.get("success") is True,
          "existing Delete Both endpoint still succeeds independently")
    check(deleted.get("archiveDeleted") is True and
          deleted.get("compressedRemoved") is True and
          deleted.get("metadataRemoved") is True,
          "Delete Both removes its separate archive and metadata")
    check(deleted.get("reportUpdated") is True,
          "Delete Both regenerates the vault report")
    check(not delete_compressed.exists() and not delete_metadata.exists() and
          not delete_both.exists(),
          "Delete Both removes the verified original and vault artifacts")
    check(restored_path.is_file() and
          restored_path.read_bytes() == expected_after_user_edit,
          "restored destination is untouched by subsequent Delete Both")
    check(active.is_file() and active.read_bytes() == active_bytes,
          "active source fixture remains unchanged through the full lifecycle")

    status, final_report = request_json(base_url, "/api/dashboard")
    check(status == 200 and final_report.get("archiveCount") == 0 and
          final_report.get("archives") == [] and
          final_report.get("compressedSize") == 0,
          "dashboard reflects the empty vault after restore and independent Delete Both")

    print("ALL PRODUCTION API LIFECYCLE TESTS PASSED")


if __name__ == "__main__":
    main()

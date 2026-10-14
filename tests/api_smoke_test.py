#!/usr/bin/env python3
"""Non-destructive HTTP smoke tests for SmartFileVault."""

import argparse
import json
import sys
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


VAULT_FIELDS = {
    "archiveCount",
    "originalSize",
    "originalSizeText",
    "compressedSize",
    "compressedSizeText",
    "spaceSaved",
    "spaceSavedText",
    "restoreSafetySpace",
    "restoreSafetySpaceText",
    "freeSpace",
    "freeSpaceText",
    "archives",
}

VAULT_NUMERIC_FIELDS = {
    "archiveCount",
    "originalSize",
    "compressedSize",
    "spaceSaved",
    "restoreSafetySpace",
    "freeSpace",
}

FILE_REPORT_FIELDS = {
    "totalFiles",
    "inactiveFiles",
    "activeFiles",
    "skippedFiles",
    "files",
}

FILE_NUMERIC_FIELDS = {
    "totalFiles",
    "inactiveFiles",
    "activeFiles",
    "skippedFiles",
}

FILE_ENTRY_FIELDS = {
    "fileName",
    "filePath",
    "extension",
    "size",
    "sizeText",
    "lastModified",
    "status",
}

SCAN_FIELDS = {
    "success",
    "scannedFiles",
    "scanSkippedFiles",
    "inactiveFiles",
    "archivedFiles",
    "skippedFiles",
    "spaceSaved",
}

SCAN_NUMERIC_FIELDS = SCAN_FIELDS - {"success"}


class SmokeTests:
    def __init__(self, base_url):
        self.base_url = base_url.rstrip("/")
        self.passed = 0
        self.failed = 0

    def check(self, name, condition, detail=""):
        if condition:
            self.passed += 1
            print(f"PASS: {name}")
        else:
            self.failed += 1
            suffix = f" — {detail}" if detail else ""
            print(f"FAIL: {name}{suffix}")

    def request(self, method, endpoint, payload=None):
        url = self.base_url + endpoint
        body = None

        headers = {}

        if payload is not None:
            body = json.dumps(payload).encode("utf-8")
            headers["Content-Type"] = "application/json"

        req = Request(
            url,
            data=body,
            headers=headers,
            method=method,
        )

        try:
            with urlopen(req, timeout=5) as response:
                status = response.status
                raw = response.read()

        except HTTPError as exc:
            status = exc.code
            raw = exc.read()

        except URLError as exc:
            reason = getattr(exc, "reason", str(exc))
            return 0, None, "", f"Connection error: {reason}"

        except TimeoutError:
            return 0, None, "", "Request timed out."

        text = raw.decode("utf-8", errors="replace")

        try:
            parsed = json.loads(text)
            parse_error = ""
        except json.JSONDecodeError as exc:
            parsed = None
            parse_error = f"Invalid JSON response: {exc}"

        return status, parsed, text, parse_error

    def request_error_detail(self, status, error, expected):
        if error:
            return error
        return f"Received HTTP {status}; expected {expected}."

    def check_vault_response(self, label, payload):
        is_object = isinstance(payload, dict)

        self.check(
            f"{label}: response is a JSON object",
            is_object,
        )

        if not is_object:
            return

        missing = VAULT_FIELDS - payload.keys()

        self.check(
            f"{label}: required fields exist",
            not missing,
            f"Missing fields: {sorted(missing)}" if missing else "",
        )

        wrong_types = [
            field
            for field in VAULT_NUMERIC_FIELDS
            if field in payload
            and (
                isinstance(payload[field], bool)
                or not isinstance(payload[field], (int, float))
            )
        ]

        self.check(
            f"{label}: numeric fields have numeric types",
            not wrong_types,
            f"Wrong types: {wrong_types}" if wrong_types else "",
        )

        self.check(
            f"{label}: archives is an array",
            isinstance(payload.get("archives"), list),
        )

        if isinstance(payload.get("archives"), list):
            for index, archive in enumerate(payload["archives"]):
                valid = (
                    isinstance(archive, dict)
                    and {
                        "archiveId",
                        "originalName",
                        "originalPath",
                        "archivePath",
                        "originalSize",
                        "originalSizeText",
                        "compressedSize",
                        "compressedSizeText",
                        "compressionPercentage",
                        "archivedAt",
                    }.issubset(archive.keys())
                )

                self.check(
                    f"{label}: archive entry {index + 1} has required fields",
                    valid,
                )

    def check_file_report(self, label, payload):
        is_object = isinstance(payload, dict)

        self.check(
            f"{label}: response is a JSON object",
            is_object,
        )

        if not is_object:
            return

        missing = FILE_REPORT_FIELDS - payload.keys()

        self.check(
            f"{label}: required fields exist",
            not missing,
            f"Missing fields: {sorted(missing)}" if missing else "",
        )

        wrong_types = [
            field
            for field in FILE_NUMERIC_FIELDS
            if field in payload
            and (
                isinstance(payload[field], bool)
                or not isinstance(payload[field], int)
            )
        ]

        self.check(
            f"{label}: count fields are integers",
            not wrong_types,
            f"Wrong types: {wrong_types}" if wrong_types else "",
        )

        entries = payload.get("files")

        self.check(
            f"{label}: files is an array",
            isinstance(entries, list),
        )

        if not isinstance(entries, list):
            return

        for index, entry in enumerate(entries):
            valid = (
                isinstance(entry, dict)
                and FILE_ENTRY_FIELDS.issubset(entry.keys())
            )

            self.check(
                f"{label}: file entry {index + 1} has required fields",
                valid,
            )

    def run(self, scan_folder=None, inactive_days=36500):
        # Dashboard
        status, data, raw, error = self.request(
            "GET", "/api/dashboard"
        )

        self.check(
            "GET /api/dashboard returns HTTP 200",
            status == 200,
            self.request_error_detail(status, error, "200"),
        )

        if status == 200:
            self.check_vault_response("Dashboard", data)

        elif error:
            self.check("Dashboard request completed", False, error)

        # Archive list
        status, data, raw, error = self.request(
            "GET", "/api/archives"
        )

        self.check(
            "GET /api/archives returns HTTP 200",
            status == 200,
            self.request_error_detail(status, error, "200"),
        )

        if status == 200:
            self.check_vault_response("Archives", data)

        elif error:
            self.check("Archives request completed", False, error)

        # The saved scan report may legitimately not exist yet.
        status, data, raw, error = self.request(
            "GET", "/api/files"
        )

        if status == 200:
            self.check("GET /api/files returns HTTP 200", True)
            self.check_file_report("Files report", data)

        elif (
            status == 404
            and isinstance(data, dict)
            and data.get("success") is False
            and isinstance(data.get("message"), str)
        ):
            self.check(
                "GET /api/files reports that no scan report is available",
                True,
            )

        else:
            self.check(
                "GET /api/files returns a valid report or an expected 404",
                False,
                self.request_error_detail(
                    status,
                    error,
                    "200 with a report, or 404 with a JSON error",
                ),
            )

        # Browser CORS preflight
        status, data, raw, error = self.request(
            "OPTIONS", "/api/scan"
        )

        self.check(
            "OPTIONS /api/scan returns HTTP 200",
            status == 200,
            self.request_error_detail(status, error, "200"),
        )

        # Missing scan parameters must be rejected.
        status, data, raw, error = self.request(
            "POST", "/api/scan", {}
        )

        self.check(
            "POST /api/scan rejects missing parameters",
            status == 400
            and isinstance(data, dict)
            and data.get("success") is False,
            self.request_error_detail(status, error, "400 with success=false"),
        )

        # An inactivity period of zero must be rejected.
        status, data, raw, error = self.request(
            "POST",
            "/api/scan",
            {"folderPath": "/tmp", "inactiveDays": 0},
        )

        self.check(
            "POST /api/scan rejects inactiveDays=0",
            status == 400
            and isinstance(data, dict)
            and data.get("success") is False,
            self.request_error_detail(status, error, "400 with success=false"),
        )

        # Missing restore fields must be rejected without restoration.
        status, data, raw, error = self.request(
            "POST", "/api/restore", {}
        )

        self.check(
            "POST /api/restore rejects missing parameters",
            status == 400
            and isinstance(data, dict)
            and data.get("success") is False,
            self.request_error_detail(status, error, "400 with success=false"),
        )

        # An unknown endpoint must not appear to succeed.
        status, data, raw, error = self.request(
            "GET", "/api/smoke-test-unknown-endpoint"
        )

        self.check(
            "Unknown endpoint returns HTTP 404",
            status == 404
            and isinstance(data, dict)
            and data.get("success") is False,
            self.request_error_detail(status, error, "404 with success=false"),
        )

        # Optional successful scan. This is deliberately opt-in because
        # the backend may archive eligible files in the selected directory.
        if scan_folder is not None:
            folder = Path(scan_folder).expanduser().resolve()

            self.check(
                "Optional scan folder exists and is a directory",
                folder.is_dir(),
                str(folder),
            )

            if folder.is_dir():
                print(
                    "\nWARNING: submitting this scan may archive eligible "
                    "files in the selected directory."
                )

                status, data, raw, error = self.request(
                    "POST",
                    "/api/scan",
                    {
                        "folderPath": str(folder),
                        "inactiveDays": inactive_days,
                    },
                )

                scan_ok = (
                    status == 200
                    and isinstance(data, dict)
                    and data.get("success") is True
                )

                self.check(
                    "POST /api/scan processes the selected folder",
                    scan_ok,
                    self.request_error_detail(
                        status,
                        error,
                        "200 with success=true",
                    ),
                )

                if scan_ok:
                    missing = SCAN_FIELDS - data.keys()

                    self.check(
                        "Scan response contains required fields",
                        not missing,
                        f"Missing fields: {sorted(missing)}" if missing else "",
                    )

                    wrong_types = [
                        field
                        for field in SCAN_NUMERIC_FIELDS
                        if field in data
                        and (
                            isinstance(data[field], bool)
                            or not isinstance(data[field], int)
                        )
                    ]

                    self.check(
                        "Scan counters are integers",
                        not wrong_types,
                        f"Wrong types: {wrong_types}" if wrong_types else "",
                    )

                    status, report, raw, error = self.request(
                        "GET", "/api/files"
                    )

                    self.check(
                        "GET /api/files returns HTTP 200 after a successful scan",
                        status == 200,
                        self.request_error_detail(status, error, "200"),
                    )

                    if status == 200:
                        self.check_file_report(
                            "Fresh scan report", report
                        )

        print(
            f"\nSummary: {self.passed} passed, {self.failed} failed."
        )

        return 1 if self.failed else 0


def main():
    parser = argparse.ArgumentParser(
        description="Run SmartFileVault HTTP API smoke tests."
    )

    parser.add_argument(
        "--base-url",
        default="http://127.0.0.1:8080",
        help="Base URL of the running API server.",
    )

    parser.add_argument(
        "--scan-folder",
        help="Optional folder to scan. This may archive eligible files.",
    )

    parser.add_argument(
        "--inactive-days",
        type=int,
        default=36500,
        help="Inactivity threshold for the optional scan (default: 36500).",
    )

    parser.add_argument(
        "--allow-scan-side-effects",
        action="store_true",
        help="Explicitly permit the optional scan to process eligible files.",
    )

    args = parser.parse_args()

    if args.scan_folder and not args.allow_scan_side_effects:
        parser.error(
            "--scan-folder may archive files. Review the folder, then add "
            "--allow-scan-side-effects to confirm."
        )

    if args.allow_scan_side_effects and not args.scan_folder:
        parser.error(
            "--allow-scan-side-effects requires --scan-folder."
        )

    if args.inactive_days <= 0:
        parser.error("--inactive-days must be greater than zero.")

    return SmokeTests(args.base_url).run(
        scan_folder=args.scan_folder,
        inactive_days=args.inactive_days,
    )


if __name__ == "__main__":
    sys.exit(main())

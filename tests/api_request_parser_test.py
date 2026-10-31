#!/usr/bin/env python3
"""Focused HTTP request-framing tests for SmartFileVault."""

import json
import socket
import sys
import tempfile
from pathlib import Path


HOST = "127.0.0.1"
PORT = 8080


def raw_request(body, *, lowercase_content_length=False):
    """Send one raw HTTP request and return its status and JSON body."""
    encoded_body = body.encode("utf-8")
    header_name = (
        "content-length"
        if lowercase_content_length
        else "Content-Length"
    )

    headers = (
        "POST /api/scan HTTP/1.1\r\n"
        f"Host: {HOST}:{PORT}\r\n"
        "Content-Type: application/json\r\n"
        f"{header_name}: {len(encoded_body)}\r\n"
        "Connection: close\r\n"
        "\r\n"
    )

    response = bytearray()

    with socket.create_connection((HOST, PORT), timeout=5) as sock:
        sock.settimeout(5)
        sock.sendall(headers.encode("ascii") + encoded_body)

        while True:
            try:
                chunk = sock.recv(4096)
            except socket.timeout:
                raise RuntimeError("Timed out waiting for HTTP response")

            if not chunk:
                break

            response.extend(chunk)

    raw = response.decode("utf-8", errors="replace")

    if "\r\n\r\n" not in raw:
        raise RuntimeError(f"Malformed HTTP response: {raw!r}")

    header_text, response_body = raw.split("\r\n\r\n", 1)
    status_line = header_text.splitlines()[0]
    status = int(status_line.split()[1])

    try:
        parsed = json.loads(response_body)
    except json.JSONDecodeError as exc:
        raise RuntimeError(
            f"Response was not valid JSON: {response_body!r}"
        ) from exc

    return status, parsed


def check(name, condition, detail=""):
    if condition:
        print(f"PASS: {name}")
        return True

    print(f"FAIL: {name}" + (f" — {detail}" if detail else ""))
    return False


def main():
    passed = 0
    failed = 0

    # An incomplete JSON body must not start a scan.
    try:
        status, data = raw_request('{"folderPath":')

        ok = (
            status == 400
            and isinstance(data, dict)
            and data.get("success") is False
        )

        passed += check(
            "Incomplete scan JSON is rejected with HTTP 400",
            ok,
            f"HTTP {status}, response={data!r}",
        )
    except Exception as exc:
        failed += 1
        print(f"FAIL: Incomplete JSON test — {exc}")

    # Test lowercase Content-Length with a fresh, isolated folder.
    try:
        with tempfile.TemporaryDirectory(
            prefix="sfv-header-test-"
        ) as directory:
            test_folder = Path(directory)
            (test_folder / "recent_file.txt").write_text(
                "Disposable parser test file.\n",
                encoding="utf-8",
            )

            payload = json.dumps({
                "folderPath": str(test_folder),
                "inactiveDays": 36500,
            })

            status, data = raw_request(
                payload,
                lowercase_content_length=True,
            )

            ok = (
                status == 200
                and isinstance(data, dict)
                and data.get("success") is True
            )

            passed += check(
                "Lowercase content-length is accepted for a valid scan",
                ok,
                f"HTTP {status}, response={data!r}",
            )

    except Exception as exc:
        failed += 1
        print(f"FAIL: Lowercase Content-Length test — {exc}")

    print(f"\nSummary: {passed} passed, {failed} failed.")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""End-to-end DELETE endpoint test for an isolated SmartFileVault API server."""

import json
import pathlib
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request


def request_json(url, method="GET", headers=None):
    request = urllib.request.Request(url, method=method, headers=headers or {})
    with urllib.request.urlopen(request, timeout=4) as response:
        return response.status, dict(response.headers), json.loads(response.read().decode())


def main():
    import sys
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python3 tests/archive_delete_api_test.py /absolute/path/to/test-server")

    binary = str(pathlib.Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="sfv-api-delete-") as temporary_root:
        root = pathlib.Path(temporary_root)
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            port = listener.getsockname()[1]

        process = subprocess.Popen(
            [binary, str(root), str(port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        try:
            id_file = root / "archive_id.txt"
            base = f"http://127.0.0.1:{port}"
            for _ in range(120):
                if id_file.exists():
                    break
                if process.poll() is not None:
                    raise AssertionError("API test server exited before creating its archive")
                time.sleep(0.05)
            assert id_file.exists(), "timed out waiting for the fixture archive ID"
            archive_id = id_file.read_text().strip()

            for _ in range(120):
                try:
                    request_json(base + "/api/archives")
                    break
                except Exception:
                    time.sleep(0.05)
            else:
                raise AssertionError("API server did not become ready")

            request = urllib.request.Request(
                base + f"/api/archives/{archive_id}",
                method="OPTIONS",
                headers={
                    "Origin": "http://127.0.0.1:8081",
                    "Access-Control-Request-Method": "DELETE",
                },
            )
            with urllib.request.urlopen(request, timeout=4) as response:
                allow_methods = response.headers.get("Access-Control-Allow-Methods", "")
            assert "DELETE" in allow_methods, allow_methods
            print("PASS: CORS preflight advertises DELETE")

            status, _, payload = request_json(
                base + f"/api/archives/{archive_id}", method="DELETE"
            )
            assert status == 200 and payload["success"] is True, payload
            assert payload["archiveDeleted"] is True
            assert payload["compressedRemoved"] is True
            assert payload["metadataRemoved"] is True
            assert payload["originalDeleted"] is True
            assert payload["originalStatus"] == "deleted"
            assert payload["reportUpdated"] is True

            vault = root / "vault"
            original = root / "originals" / "api-delete.txt"
            assert not list((vault / "compressed").glob(archive_id + "_*.gz"))
            assert not (vault / "metadata" / f"{archive_id}.meta").exists()
            assert not original.exists()
            print("PASS: DELETE removes the matching compressed archive, metadata, and verified original")

            try:
                request_json(base + f"/api/archives/{archive_id}", method="DELETE")
                raise AssertionError("repeat delete unexpectedly succeeded")
            except urllib.error.HTTPError as error:
                payload = json.loads(error.read().decode())
                assert error.code == 404 and payload["errorCode"] == "archive_not_found", payload
            print("PASS: repeated DELETE returns 404 archive_not_found")

            try:
                request_json(base + "/api/archives/%2E%2E%2Foutside", method="DELETE")
                raise AssertionError("encoded traversal archive ID unexpectedly succeeded")
            except urllib.error.HTTPError as error:
                payload = json.loads(error.read().decode())
                assert error.code == 400 and payload["errorCode"] == "invalid_archive_id", payload
            print("PASS: encoded traversal archive ID is rejected")

            _, _, report = request_json(base + "/api/archives")
            assert report.get("archives") == [] and report.get("archiveCount") == 0, report
            print("PASS: GET /api/archives reflects deletion after report regeneration")
            print("ALL ARCHIVE DELETE API TESTS PASSED")
        finally:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            if process.stdout:
                output = process.stdout.read()
                if output.strip():
                    print("API SERVER LOG:", output[-1200:])


if __name__ == "__main__":
    main()

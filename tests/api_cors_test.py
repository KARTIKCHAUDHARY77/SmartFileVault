#!/usr/bin/env python3
"""Check that the local API only grants browser CORS access to its local UI."""

import http.client
import json
import os
import pathlib
import tempfile
import time

API_HOST = "127.0.0.1"
API_PORT = 8080
TRUSTED_ORIGIN = "http://127.0.0.1:8081"
UNTRUSTED_ORIGIN = "https://untrusted.example"


def request(method: str, path: str, origin: str, body=None, content_type=None):
    connection = http.client.HTTPConnection(API_HOST, API_PORT, timeout=5)
    try:
        headers = {"Origin": origin}
        if method == "OPTIONS":
            headers["Access-Control-Request-Method"] = "DELETE"
            headers["Access-Control-Request-Headers"] = "content-type"
        if body is not None:
            headers["Content-Type"] = content_type or "application/json"
        connection.request(method, path, body=body, headers=headers)
        response = connection.getresponse()
        response_body = response.read()
        return response.status, dict(response.getheaders()), response_body
    finally:
        connection.close()


def header(headers, name):
    for key, value in headers.items():
        if key.lower() == name.lower():
            return value
    return None


def main() -> int:
    status, headers, _ = request("OPTIONS", "/api/archives/example", TRUSTED_ORIGIN)
    assert status == 200, f"trusted preflight returned HTTP {status}"
    assert header(headers, "Access-Control-Allow-Origin") == TRUSTED_ORIGIN
    methods = header(headers, "Access-Control-Allow-Methods") or ""
    assert "DELETE" in methods
    allowed_headers = header(headers, "Access-Control-Allow-Headers") or ""
    assert "Content-Type" in allowed_headers
    print("PASS: local frontend origin can preflight DELETE")

    status, headers, _ = request("GET", "/api/dashboard", TRUSTED_ORIGIN)
    assert status == 200, f"dashboard returned HTTP {status}"
    assert header(headers, "Access-Control-Allow-Origin") == TRUSTED_ORIGIN
    print("PASS: local frontend origin can read dashboard JSON")

    status, headers, _ = request("OPTIONS", "/api/archives/example", UNTRUSTED_ORIGIN)
    assert status == 403, f"untrusted preflight returned HTTP {status}, expected 403"
    assert header(headers, "Access-Control-Allow-Origin") is None
    print("PASS: untrusted website receives no CORS permission")

    status, headers, _ = request("GET", "/api/dashboard", UNTRUSTED_ORIGIN)
    assert status == 403, f"untrusted GET returned HTTP {status}, expected 403"
    assert header(headers, "Access-Control-Allow-Origin") is None
    print("PASS: untrusted website requests are rejected")

    with tempfile.TemporaryDirectory(prefix="sfv-cors-side-effect-") as temporary:
        fixture = pathlib.Path(temporary) / "should-not-be-archived.txt"
        fixture.write_text("Untrusted cross-origin scan must not touch this file.\\n", encoding="utf-8")
        old_time = time.time() - (3 * 24 * 60 * 60)
        os.utime(fixture, (old_time, old_time))

        status, headers, _ = request(
            "POST",
            "/api/scan",
            UNTRUSTED_ORIGIN,
            body=json.dumps({"folderPath": str(pathlib.Path(temporary)), "inactiveDays": 1}),
            content_type="text/plain",
        )
        assert status == 403, f"untrusted simple POST returned HTTP {status}, expected 403"
        assert header(headers, "Access-Control-Allow-Origin") is None
        assert fixture.is_file(), "untrusted POST changed or removed the source fixture"
        assert fixture.read_text(encoding="utf-8").startswith("Untrusted cross-origin")
        print("PASS: untrusted simple POST is rejected before scan routing")
        print("PASS: rejected cross-origin POST leaves filesystem fixture unchanged")

    print("ALL API CORS TESTS PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

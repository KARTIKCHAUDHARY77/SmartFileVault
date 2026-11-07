#!/usr/bin/env python3
"""Check that the local API only grants browser CORS access to its local UI."""

import http.client

API_HOST = "127.0.0.1"
API_PORT = 8080
TRUSTED_ORIGIN = "http://127.0.0.1:8081"
UNTRUSTED_ORIGIN = "https://untrusted.example"


def request(method: str, path: str, origin: str):
    connection = http.client.HTTPConnection(API_HOST, API_PORT, timeout=5)
    try:
        headers = {"Origin": origin}
        if method == "OPTIONS":
            headers["Access-Control-Request-Method"] = "DELETE"
            headers["Access-Control-Request-Headers"] = "content-type"
        connection.request(method, path, headers=headers)
        response = connection.getresponse()
        body = response.read()
        return response.status, dict(response.getheaders()), body
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
    assert status == 200, f"untrusted preflight HTTP status was {status}"
    assert header(headers, "Access-Control-Allow-Origin") is None
    print("PASS: untrusted website receives no CORS permission")

    status, headers, _ = request("GET", "/api/dashboard", UNTRUSTED_ORIGIN)
    assert status == 200, f"untrusted GET returned HTTP {status}"
    assert header(headers, "Access-Control-Allow-Origin") is None
    print("PASS: untrusted website cannot read local dashboard response")

    print("ALL API CORS TESTS PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

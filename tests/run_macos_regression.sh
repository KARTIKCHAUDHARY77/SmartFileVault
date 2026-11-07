#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CXX="${CXX:-clang++}"
PYTHON="${PYTHON:-python3}"
OPENSSL_PREFIX="${OPENSSL_PREFIX:-$(brew --prefix openssl@3)}"

if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "ERROR: C++ compiler not found: $CXX" >&2
    exit 1
fi

if ! command -v "$PYTHON" >/dev/null 2>&1; then
    echo "ERROR: Python 3 not found: $PYTHON" >&2
    exit 1
fi

if [[ ! -d "$OPENSSL_PREFIX/include" || ! -d "$OPENSSL_PREFIX/lib" ]]; then
    echo "ERROR: OpenSSL 3 headers/libraries not found at: $OPENSSL_PREFIX" >&2
    exit 1
fi

BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/sfv-ci-build.XXXXXX")"
RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/sfv-ci-run.XXXXXX")"
SERVER_PID=""

cleanup() {
    if [[ -n "$SERVER_PID" ]]; then
        kill "$SERVER_PID" >/dev/null 2>&1 || true
        wait "$SERVER_PID" >/dev/null 2>&1 || true
    fi
    rm -rf "$BUILD_DIR" "$RUN_DIR"
}
trap cleanup EXIT

COMMON_FLAGS=(
    -std=c++17
    -Wall
    -Wextra
    -Wpedantic
    -Isrc
    "-I$OPENSSL_PREFIX/include"
)

LINK_FLAGS=(
    "-L$OPENSSL_PREFIX/lib"
    -lcrypto
    -lz
    -pthread
)

build_test() {
    local name="$1"
    shift
    echo
    echo "========== BUILD: $name =========="
    "$CXX" "${COMMON_FLAGS[@]}" "$@" "${LINK_FLAGS[@]}" \
        -o "$BUILD_DIR/$name"
}

run_test() {
    local name="$1"
    local work_dir="$RUN_DIR/$name"
    mkdir -p "$work_dir"
    echo
    echo "========== RUN: $name =========="
    (cd "$work_dir" && "$BUILD_DIR/$name")
}

# Core compression behavior and byte-exact round trips.
build_test compression_test \
    src/compression_test.cpp \
    src/compression.cpp
run_test compression_test

# Extension-independent compression eligibility and invalid-file rejection.
build_test compression_policy_test \
    src/compression_policy_test.cpp \
    src/compression_policy.cpp
run_test compression_policy_test

# Strict expected-size GZIP decompression and malformed-stream rejection.
build_test decompression_limit_test \
    src/decompression_limit_test.cpp \
    src/compression.cpp
run_test decompression_limit_test

# SHA-256 propagation, verification, legacy call compatibility, and no-overwrite behavior.
build_test compression_sha256_test \
    src/compression_sha256_test.cpp \
    src/compression.cpp
run_test compression_sha256_test

# Compile the real web-app API entry point (not the disposable test harness).
build_test sfv_api_server \\
    src/api_server_main.cpp \\
    src/api_server.cpp \\
    src/application_service.cpp \\
    src/file_manager.cpp \\
    src/inactivity.cpp \\
    src/scan_report.cpp \\
    src/vault_service.cpp \\
    src/archive_service.cpp \\
    src/archive.cpp \\
    src/compression.cpp \\
    src/compression_policy.cpp \\
    src/file_utils.cpp \\
    src/restore.cpp \\
    src/restore_safety.cpp \\
    src/vault_report.cpp
"$BUILD_DIR/sfv_api_server" --help

# The vault report must remain valid JSON even when a legal filename contains
# tabs or quotes (both are legal characters on macOS/POSIX filesystems).
build_test vault_report_json_test \\
    src/vault_report_json_test.cpp \\
    src/vault_report.cpp \\
    src/vault_service.cpp \\
    src/archive_service.cpp \\
    src/archive.cpp \\
    src/compression.cpp \\
    src/compression_policy.cpp \\
    src/file_utils.cpp \\
    src/restore.cpp \\
    src/restore_safety.cpp
"$PYTHON" tests/vault_report_json_test.py --binary "$BUILD_DIR/vault_report_json_test"

# Validate the local-origin CORS policy and preflight support.
bash -n scripts/run_smartfilevault.sh

# Build the real API server with the production application/service sources.
echo
echo "========== BUILD: API SERVER =========="
"$CXX" "${COMMON_FLAGS[@]}" \
    src/api_server_test.cpp \
    src/api_server.cpp \
    src/application_service.cpp \
    src/file_manager.cpp \
    src/inactivity.cpp \
    src/scan_report.cpp \
    src/vault_service.cpp \
    src/archive_service.cpp \
    src/archive.cpp \
    src/compression.cpp \
    src/compression_policy.cpp \
    src/file_utils.cpp \
    src/restore.cpp \
    src/restore_safety.cpp \
    src/vault_report.cpp \
    "${LINK_FLAGS[@]}" \
    -o "$BUILD_DIR/sfv_api_server"

# Use a disposable working directory because the server fixture writes files
# relative to its current directory. The parser test's successful scan uses a
# 36,500-day threshold and a disposable one-file directory.
API_WORK_DIR="$RUN_DIR/api-server"
mkdir -p "$API_WORK_DIR"

if lsof -nP -iTCP:8080 -sTCP:LISTEN >/dev/null 2>&1; then
    echo "ERROR: TCP port 8080 is already in use; refusing to test against another process." >&2
    exit 1
fi

cd "$API_WORK_DIR"
"$BUILD_DIR/sfv_api_server" >"$RUN_DIR/api-server.log" 2>&1 &
SERVER_PID="$!"
cd "$ROOT"

echo
echo "========== WAIT FOR API SERVER =========="
API_READY=0
for attempt in $(seq 1 100); do
    if ! kill -0 "$SERVER_PID" >/dev/null 2>&1; then
        echo "ERROR: API server exited during startup." >&2
        cat "$RUN_DIR/api-server.log" >&2
        exit 1
    fi
    if curl --silent --show-error --fail \
        http://127.0.0.1:8080/api/archives >/dev/null 2>&1; then
        API_READY=1
        break
    fi
    sleep 0.2
done

if [[ "$API_READY" -ne 1 ]]; then
    echo "ERROR: API server did not become ready on port 8080." >&2
    cat "$RUN_DIR/api-server.log" >&2
    exit 1
fi

echo
echo "========== API HTTP SMOKE TESTS =========="
"$PYTHON" tests/api_smoke_test.py \
    --base-url http://127.0.0.1:8080

echo
echo "========== API REQUEST PARSER TESTS =========="
"$PYTHON" tests/api_request_parser_test.py

echo
echo "========== API CORS ALLOWLIST TESTS =========="
"$PYTHON" tests/api_cors_test.py

echo
echo "========== FRONTEND JAVASCRIPT SYNTAX =========="
if command -v node >/dev/null 2>&1; then
    node --check frontend/script.js
else
    echo "ERROR: Node.js is required for the frontend syntax check." >&2
    exit 1
fi

echo
echo "========== MACOS REGRESSION SMOKE SUITE PASSED =========="

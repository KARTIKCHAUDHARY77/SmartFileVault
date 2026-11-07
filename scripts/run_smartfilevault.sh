#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
CXX="${CXX:-clang++}"
PYTHON="${PYTHON:-python3}"
OPENSSL_PREFIX="${OPENSSL_PREFIX:-$(brew --prefix openssl@3)}"
VAULT_ROOT="${SFV_VAULT_ROOT:-$HOME/Library/Application Support/SmartFileVault/Vault}"
API_PORT=8080
UI_PORT=8081

for command_name in "$CXX" "$PYTHON" curl lsof; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "ERROR: Required command not found: $command_name" >&2
        exit 1
    fi
done

if [[ "${SFV_SKIP_OPEN:-0}" != "1" ]] &&
   ! command -v open >/dev/null 2>&1; then
    echo "ERROR: Required command not found: open" >&2
    exit 1
fi

if [[ ! -d "$OPENSSL_PREFIX/include" || ! -d "$OPENSSL_PREFIX/lib" ]]; then
    echo "ERROR: OpenSSL 3 was not found at $OPENSSL_PREFIX" >&2
    echo "Install it with: brew install openssl@3" >&2
    exit 1
fi

if lsof -nP -iTCP:"$API_PORT" -sTCP:LISTEN >/dev/null 2>&1; then
    echo "ERROR: Port $API_PORT is already in use. Stop the other API server first." >&2
    exit 1
fi

if lsof -nP -iTCP:"$UI_PORT" -sTCP:LISTEN >/dev/null 2>&1; then
    echo "ERROR: Port $UI_PORT is already in use. Stop the other local web server first." >&2
    exit 1
fi

mkdir -p "$VAULT_ROOT"
VAULT_ROOT="$(cd "$VAULT_ROOT" && pwd -P)"

BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/smartfilevault-app.XXXXXX")"
API_PID=""
UI_PID=""

cleanup() {
    if [[ -n "$UI_PID" ]]; then
        kill "$UI_PID" >/dev/null 2>&1 || true
        wait "$UI_PID" >/dev/null 2>&1 || true
    fi
    if [[ -n "$API_PID" ]]; then
        kill "$API_PID" >/dev/null 2>&1 || true
        wait "$API_PID" >/dev/null 2>&1 || true
    fi
    rm -rf "$BUILD_DIR"
}
trap cleanup EXIT
trap 'exit 0' INT TERM

echo "========== BUILD SMARTFILEVAULT WEB APP =========="
"$CXX" -std=c++17 -Wall -Wextra -Wpedantic -Isrc \
    "-I$OPENSSL_PREFIX/include" \
    src/api_server_main.cpp \
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
    "-L$OPENSSL_PREFIX/lib" -lcrypto -lz -pthread \
    -o "$BUILD_DIR/sfv_api_server"

echo "========== START LOCAL API =========="
SFV_VAULT_ROOT="$VAULT_ROOT" "$BUILD_DIR/sfv_api_server" \
    >"$BUILD_DIR/api.log" 2>&1 &
API_PID="$!"

echo "========== START LOCAL FRONTEND =========="
"$PYTHON" -m http.server "$UI_PORT" --bind 127.0.0.1 \
    --directory "$ROOT/frontend" >"$BUILD_DIR/frontend.log" 2>&1 &
UI_PID="$!"

ready=0
for attempt in $(seq 1 100); do
    if ! kill -0 "$API_PID" >/dev/null 2>&1; then
        echo "ERROR: API server exited during startup." >&2
        cat "$BUILD_DIR/api.log" >&2
        exit 1
    fi
    if ! kill -0 "$UI_PID" >/dev/null 2>&1; then
        echo "ERROR: Frontend server exited during startup." >&2
        cat "$BUILD_DIR/frontend.log" >&2
        exit 1
    fi
    if curl --silent --show-error --fail \
        "http://127.0.0.1:$UI_PORT/" >/dev/null 2>&1 &&
       curl --silent --show-error --fail \
        "http://127.0.0.1:$API_PORT/api/archives" >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 0.2
done

if [[ "$ready" -ne 1 ]]; then
    echo "ERROR: SmartFileVault did not become ready." >&2
    cat "$BUILD_DIR/api.log" >&2
    cat "$BUILD_DIR/frontend.log" >&2
    exit 1
fi

echo
echo "SmartFileVault is running."
echo "Frontend: http://127.0.0.1:$UI_PORT/"
echo "API:      http://127.0.0.1:$API_PORT/"
echo "Vault:    $VAULT_ROOT"
echo
echo "Scanning can archive files and remove originals after verification."
echo "Use a disposable test folder for your first scan."
echo "Press Ctrl+C here to stop both local servers."
if [[ "${SFV_SKIP_OPEN:-0}" != "1" ]]; then
    open "http://127.0.0.1:$UI_PORT/"
fi
wait "$API_PID"

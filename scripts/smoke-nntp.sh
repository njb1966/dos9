#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
LOG_FILE="$(mktemp /tmp/dos9-nntp-smoke.XXXXXX)"
SERVER_LOG="$(mktemp /tmp/dos9-nntp-server.XXXXXX)"
SERVER_PID=""

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$LOG_FILE" "$SERVER_LOG"
}
trap cleanup EXIT

fail() {
    cat "$SERVER_LOG"
    cat "$LOG_FILE"
    exit 1
}

bash "$SCRIPT_DIR/nntp-server.sh" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 1

set +e
( sleep 2; printf '%s\n' "/disk/nntp 10.0.2.100 1234 comp.test 1" ) | \
    timeout 20 "$SCRIPT_DIR/qemu.sh" guestfwd >"$LOG_FILE" 2>&1
rc=$?
set -e

if [ "$rc" -ne 0 ] && [ "$rc" -ne 124 ]; then
    fail
fi

grep -F "guestfwd nntp ok" "$LOG_FILE" >/dev/null || fail
grep -F "hello world" "$LOG_FILE" >/dev/null || fail

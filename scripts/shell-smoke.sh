#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
LOG_FILE="$(mktemp /tmp/dos9-shell-smoke.XXXXXX)"

cleanup() {
    rm -f "$LOG_FILE"
}
trap cleanup EXIT

set +e
( sleep 2; cat "$ROOT_DIR/tests/shellreg.txt" ) | timeout 20 "$SCRIPT_DIR/qemu.sh" user >"$LOG_FILE" 2>&1
rc=$?
set -e

if [ "$rc" -ne 0 ] && [ "$rc" -ne 124 ]; then
    cat "$LOG_FILE"
    exit "$rc"
fi

grep -F "shellreg start" "$LOG_FILE" >/dev/null
grep -F "name:    hello" "$LOG_FILE" >/dev/null
grep -F "crc32:" "$LOG_FILE" >/dev/null
grep -F "if-ok" "$LOG_FILE" >/dev/null
grep -F "alpha" "$LOG_FILE" >/dev/null
grep -F "beta gamma" "$LOG_FILE" >/dev/null
grep -F "a b" "$LOG_FILE" >/dev/null
grep -Eq '[0-9]{10}' "$LOG_FILE"
grep -F "hex=00000001" "$LOG_FILE" >/dev/null
grep -F "dec=00042" "$LOG_FILE" >/dev/null
grep -F "neg=   -42" "$LOG_FILE" >/dev/null
grep -F "calloc overflow guarded" "$LOG_FILE" >/dev/null
grep -F "argv[1]=" "$LOG_FILE" >/dev/null
grep -F "argv[2]=tail" "$LOG_FILE" >/dev/null
grep -F "argc=4" "$LOG_FILE" >/dev/null
grep -F "two words" "$LOG_FILE" >/dev/null
grep -F "installed: hello" "$LOG_FILE" >/dev/null
grep -F "Hello from user space!" "$LOG_FILE" >/dev/null
grep -F "shellreg end" "$LOG_FILE" >/dev/null

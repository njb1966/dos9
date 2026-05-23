#!/usr/bin/env bash
set -e

# One-shot Gopher-style response for the guestfwd smoke test.
# This exits after the first connection closes.
printf 'i guestfwd ok\t\t\t\r\n0hello\t/hello\t127.0.0.1\t1234\r\n.\r\n' | nc -nlv -s 127.0.0.1 -p 1234

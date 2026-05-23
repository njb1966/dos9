#!/usr/bin/env bash
set -e

# One-shot IRC response for the guestfwd smoke test.
printf ':irc.example 001 dos9 :guestfwd irc ok\r\n:irc.example PRIVMSG #dos9 :hello\r\n' | nc -nlv -s 127.0.0.1 -p 1234

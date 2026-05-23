#!/usr/bin/env bash
set -e

# One-shot Finger response for the guestfwd smoke test.
python3 -u -c '
import socket

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", 1234))
srv.listen(1)
conn, _ = srv.accept()
try:
    conn.recv(1024)
    conn.sendall(b"guestfwd finger ok\nhello\n")
finally:
    conn.close()
    srv.close()
'

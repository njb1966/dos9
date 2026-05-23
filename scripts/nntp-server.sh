#!/usr/bin/env bash
set -e

# One-shot NNTP response for the guestfwd smoke test.
python3 -u -c '
import socket

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", 1234))
srv.listen(1)
conn, _ = srv.accept()
try:
    conn.recv(1024)
    conn.sendall(
        b"200 guestfwd nntp ok\r\n"
        b"201 posting allowed\r\n"
        b"211 1 1 1 comp.test\r\n"
        b"220 1 <1@example.invalid> article follows\r\n"
        b"Subject: hello\r\n"
        b"\r\n"
        b"hello world\r\n"
        b".\r\n"
        b"205 closing connection\r\n"
    )
finally:
    conn.close()
    srv.close()
'

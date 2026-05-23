#!/usr/bin/env bash
set -e

# One-shot HTTP response for the RSS guestfwd smoke test.
# This exits after the first connection closes.
cat <<'EOF' | nc -N -nlv -s 127.0.0.1 -p 1234
HTTP/1.0 200 OK
Content-Type: application/rss+xml
Content-Length: 207
Connection: close

<?xml version="1.0"?>
<rss version="2.0">
  <channel>
    <title>guestfwd rss ok</title>
    <item>
      <title>hello</title>
      <link>http://example.invalid/hello</link>
    </item>
  </channel>
</rss>
EOF

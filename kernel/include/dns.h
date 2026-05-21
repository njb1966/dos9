#pragma once
#include <stdint.h>

/* Register UDP handler for DNS replies. Called from net_init(). */
void dns_init(void);

/* Synchronous A-record lookup against the QEMU SLIRP DNS server (10.0.2.3).
   Blocks via schedule() until a reply arrives or ~3 s timeout.
   Returns 0 on success with *ip set (host byte order), -1 on failure. */
int dns_lookup(const char *hostname, uint32_t *ip);

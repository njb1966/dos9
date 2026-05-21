#pragma once
#include <stdint.h>

/* Kick off DHCP DISCOVER (non-blocking — result arrives via UDP IRQ). */
void dhcp_start(void);

/* Returns 1 once DHCP ACK has been processed and g_netif is configured. */
int dhcp_done(void);

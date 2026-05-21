#pragma once
#include <stdint.h>

/* Probe PCI for an RTL8139, init the NIC, register IRQ handler, and
   populate g_netif.mac + g_netif.send.  Returns 1 if found, 0 if not. */
int rtl8139_init(void);

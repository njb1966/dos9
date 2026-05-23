#pragma once
#include <stdint.h>

/* Receive an ARP packet (payload past Ethernet header). */
void arp_rx(const void *pkt, uint16_t len);

/* Resolve IP → MAC.  Returns pointer to 6-byte MAC in cache, or NULL.
   Sends an ARP request if the entry is not cached (caller must retry). */
const uint8_t *arp_resolve(uint32_t ip);

/* Force-add a static entry (e.g. for the gateway after DHCP). */
void arp_add(uint32_t ip, const uint8_t *mac);

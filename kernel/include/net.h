#pragma once
#include <stdint.h>

/* Byte-order helpers (x86 is little-endian; network is big-endian). */
static inline uint16_t htons(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}
static inline uint16_t ntohs(uint16_t v) { return htons(v); }

static inline uint32_t htonl(uint32_t v) {
    return ((v & 0x000000FFu) << 24)
         | ((v & 0x0000FF00u) <<  8)
         | ((v & 0x00FF0000u) >>  8)
         | ((v & 0xFF000000u) >> 24);
}
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

/* 16-bit one's-complement checksum over `len` bytes of `data`. */
static inline uint16_t ip_checksum(const void *data, uint32_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t       sum = 0;
    while (len > 1) {
        sum += (uint16_t)((uint16_t)p[0] << 8 | p[1]);
        p += 2;
        len -= 2;
    }
    if (len) sum += (uint16_t)((uint16_t)p[0] << 8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/* Build a 32-bit IPv4 address from four octets (host byte order). */
#define IP4(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(d))

#define MAC_BCAST "\xff\xff\xff\xff\xff\xff"

#pragma once
#include <stdint.h>

#define IP_PROTO_ICMP  1
#define IP_PROTO_TCP   6
#define IP_PROTO_UDP   17

typedef struct {
    uint8_t  version_ihl;  /* version=4, IHL=5 → 0x45 */
    uint8_t  tos;
    uint16_t total_len;    /* network byte order */
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src;          /* network byte order */
    uint32_t dst;
} __attribute__((packed)) ip_hdr_t;

#define IP_HDR_LEN 20

void ip_rx(const void *pkt, uint16_t len);

/* Send an IP packet to dst_ip with given protocol and payload. */
void ip_send(uint32_t dst_ip, uint8_t proto,
             const void *payload, uint16_t plen);

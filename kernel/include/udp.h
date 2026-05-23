#pragma once
#include <stdint.h>

#define UDP_HDR_LEN 8

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_hdr_t;

typedef void (*udp_handler_t)(const void *payload, uint16_t len,
                              uint32_t src_ip, uint16_t src_port);

/* Register a handler for incoming UDP on dst_port (host byte order). */
void udp_register(uint16_t dst_port, udp_handler_t h);

void udp_rx(const void *pkt, uint16_t len, uint32_t src_ip, uint32_t dst_ip);

/* Send a UDP datagram. */
void udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const void *payload, uint16_t plen);

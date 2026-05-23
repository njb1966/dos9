#pragma once
#include <stdint.h>

#define ETH_HDR_LEN  14
#define ETH_TYPE_IP  0x0800
#define ETH_TYPE_ARP 0x0806

typedef struct {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t type;   /* network byte order */
} __attribute__((packed)) eth_hdr_t;

/* Receive: dispatch by EtherType. */
void ethernet_rx(const void *frame, uint16_t len);

/* Send: prepend Ethernet header and call netif_send. */
void ethernet_send(const uint8_t *dst_mac, uint16_t ethertype,
                   const void *payload, uint16_t plen);

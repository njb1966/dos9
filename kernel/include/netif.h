#pragma once
#include <stdint.h>

/* The one global network interface. */
typedef struct {
    uint8_t  mac[6];
    uint32_t ip;       /* host byte order */
    uint32_t netmask;
    uint32_t gateway;
    int      up;       /* 1 once DHCP completes */
    void (*send)(const void *frame, uint16_t len);
} netif_t;

extern netif_t g_netif;

/* Called by the NIC driver for every received Ethernet frame. */
void netif_receive(const void *frame, uint16_t len);

/* Called by upper layers to send an Ethernet frame. */
void netif_send(const void *frame, uint16_t len);

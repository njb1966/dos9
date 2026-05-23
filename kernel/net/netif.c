#include <netif.h>
#include <ethernet.h>

netif_t g_netif;

void netif_receive(const void *frame, uint16_t len) {
    ethernet_rx(frame, len);
}

void netif_send(const void *frame, uint16_t len) {
    if (g_netif.send)
        g_netif.send(frame, len);
}

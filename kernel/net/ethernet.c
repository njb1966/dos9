#include <ethernet.h>
#include <netif.h>
#include <arp.h>
#include <ip.h>
#include <net.h>
#include <terminal.h>
#include <string.h>

void ethernet_rx(const void *frame, uint16_t len) {
    if (len < ETH_HDR_LEN) return;
    const eth_hdr_t *eh  = (const eth_hdr_t *)frame;
    const uint8_t   *pay = (const uint8_t *)frame + ETH_HDR_LEN;
    uint16_t         plen = (uint16_t)(len - ETH_HDR_LEN);
    uint16_t         type = ntohs(eh->type);

    if (type == ETH_TYPE_ARP || type == ETH_TYPE_IP) {
        terminal_write("[ETHRX] type=");
        terminal_writehex(type);
        terminal_write(" len=");
        terminal_writedec(len);
        terminal_write("\n");
    }

    switch (type) {
    case ETH_TYPE_ARP: arp_rx(pay, plen);        break;
    case ETH_TYPE_IP:  ip_rx(pay, plen);          break;
    default:                                       break;
    }
}

void ethernet_send(const uint8_t *dst_mac, uint16_t ethertype,
                   const void *payload, uint16_t plen) {
    uint8_t frame[1514];
    if ((uint32_t)(ETH_HDR_LEN + plen) > sizeof(frame)) return;

    eth_hdr_t *eh = (eth_hdr_t *)frame;
    memcpy(eh->dst, dst_mac,       6);
    memcpy(eh->src, g_netif.mac,   6);
    eh->type = htons(ethertype);
    memcpy(frame + ETH_HDR_LEN, payload, plen);

    netif_send(frame, (uint16_t)(ETH_HDR_LEN + plen));
}

#include <ip.h>
#include <icmp.h>
#include <udp.h>
#include <tcp.h>
#include <arp.h>
#include <ethernet.h>
#include <netif.h>
#include <net.h>
#include <string.h>

static uint16_t g_ip_id = 1;

void ip_rx(const void *pkt, uint16_t len) {
    if (len < IP_HDR_LEN) return;
    const ip_hdr_t *ih = (const ip_hdr_t *)pkt;
    uint8_t  ihl   = (ih->version_ihl & 0x0F) * 4;
    uint16_t total = ntohs(ih->total_len);
    if (total > len || ihl < IP_HDR_LEN) return;

    uint32_t src_ip  = ntohl(ih->src);
    const uint8_t *pay  = (const uint8_t *)pkt + ihl;
    uint16_t       plen = (uint16_t)(total - ihl);

    switch (ih->protocol) {
    case IP_PROTO_ICMP: icmp_rx(pay, plen, src_ip);                    break;
    case IP_PROTO_UDP:  udp_rx(pay, plen, src_ip, ntohl(ih->dst));     break;
    case IP_PROTO_TCP:  tcp_rx(pay, plen, src_ip, ntohl(ih->dst));     break;
    }
}

void ip_send(uint32_t dst_ip, uint8_t proto,
             const void *payload, uint16_t plen) {
    uint8_t frame[1500];
    if ((uint32_t)(IP_HDR_LEN + plen) > sizeof(frame)) return;

    ip_hdr_t *ih = (ip_hdr_t *)frame;
    ih->version_ihl = 0x45;
    ih->tos         = 0;
    ih->total_len   = htons((uint16_t)(IP_HDR_LEN + plen));
    ih->id          = htons(g_ip_id++);
    ih->frag_off    = 0;
    ih->ttl         = 64;
    ih->protocol    = proto;
    ih->checksum    = 0;
    ih->src         = htonl(g_netif.ip);
    ih->dst         = htonl(dst_ip);
    ih->checksum    = ip_checksum(ih, IP_HDR_LEN);

    memcpy(frame + IP_HDR_LEN, payload, plen);

    /* Determine destination MAC via ARP.  For broadcast/DHCP use BCAST. */
    const uint8_t *dst_mac;
    if (dst_ip == 0xFFFFFFFFu || g_netif.ip == 0) {
        dst_mac = (const uint8_t *)MAC_BCAST;
    } else {
        /* Use gateway MAC for off-subnet destinations. */
        uint32_t next_hop = dst_ip;
        if ((dst_ip & g_netif.netmask) != (g_netif.ip & g_netif.netmask))
            next_hop = g_netif.gateway;

        dst_mac = arp_resolve(next_hop);
        if (!dst_mac) {
            /* ARP request was sent; caller may retry after a schedule(). */
            return;
        }
    }

    ethernet_send(dst_mac, ETH_TYPE_IP, frame, (uint16_t)(IP_HDR_LEN + plen));
}

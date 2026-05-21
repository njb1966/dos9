#include <udp.h>
#include <ip.h>
#include <net.h>
#include <string.h>

#define UDP_HANDLERS 4

static struct { uint16_t port; udp_handler_t fn; } g_handlers[UDP_HANDLERS];

void udp_register(uint16_t dst_port, udp_handler_t h) {
    for (int i = 0; i < UDP_HANDLERS; i++) {
        if (!g_handlers[i].fn) {
            g_handlers[i].port = dst_port;
            g_handlers[i].fn   = h;
            return;
        }
    }
}

void udp_rx(const void *pkt, uint16_t len, uint32_t src_ip, uint32_t dst_ip) {
    (void)dst_ip;
    if (len < UDP_HDR_LEN) return;
    const udp_hdr_t *uh  = (const udp_hdr_t *)pkt;
    uint16_t dport       = ntohs(uh->dst_port);
    uint16_t sport       = ntohs(uh->src_port);
    const uint8_t *pay   = (const uint8_t *)pkt + UDP_HDR_LEN;
    uint16_t       plen  = (uint16_t)(ntohs(uh->length) - UDP_HDR_LEN);

    for (int i = 0; i < UDP_HANDLERS; i++) {
        if (g_handlers[i].fn && g_handlers[i].port == dport) {
            g_handlers[i].fn(pay, plen, src_ip, sport);
            return;
        }
    }
}

void udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const void *payload, uint16_t plen) {
    uint8_t buf[1472];   /* max UDP payload in a 1500-byte Ethernet frame */
    if ((uint32_t)(UDP_HDR_LEN + plen) > sizeof(buf)) return;

    udp_hdr_t *uh = (udp_hdr_t *)buf;
    uh->src_port  = htons(src_port);
    uh->dst_port  = htons(dst_port);
    uh->length    = htons((uint16_t)(UDP_HDR_LEN + plen));
    uh->checksum  = 0;   /* UDP checksum is optional for IPv4 */
    memcpy(buf + UDP_HDR_LEN, payload, plen);

    ip_send(dst_ip, IP_PROTO_UDP, buf, (uint16_t)(UDP_HDR_LEN + plen));
}

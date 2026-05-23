#include <udp.h>
#include <ip.h>
#include <netif.h>
#include <net.h>
#include <terminal.h>
#include <string.h>

#define UDP_HANDLERS 4

static void log_ip4(uint32_t ip) {
    terminal_writedec((ip >> 24) & 0xFF); terminal_write(".");
    terminal_writedec((ip >> 16) & 0xFF); terminal_write(".");
    terminal_writedec((ip >>  8) & 0xFF); terminal_write(".");
    terminal_writedec(ip & 0xFF);
}

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
    if (len < UDP_HDR_LEN) return;
    const udp_hdr_t *uh  = (const udp_hdr_t *)pkt;
    uint16_t dport       = ntohs(uh->dst_port);
    uint16_t sport       = ntohs(uh->src_port);
    uint16_t udplen      = ntohs(uh->length);
    if (udplen < UDP_HDR_LEN || udplen > len) return;
    const uint8_t *pay   = (const uint8_t *)pkt + UDP_HDR_LEN;
    uint16_t       plen  = (uint16_t)(udplen - UDP_HDR_LEN);

    terminal_write("[UDPRX] ");
    log_ip4(src_ip);
    terminal_write(":");
    terminal_writedec(sport);
    terminal_write(" -> ");
    log_ip4(dst_ip);
    terminal_write(":");
    terminal_writedec(dport);
    terminal_write(" len=");
    terminal_writedec(plen);
    terminal_write("\n");

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

    terminal_write("[UDPTX] ");
    log_ip4(g_netif.ip);
    terminal_write(":");
    terminal_writedec(src_port);
    terminal_write(" -> ");
    log_ip4(dst_ip);
    terminal_write(":");
    terminal_writedec(dst_port);
    terminal_write(" len=");
    terminal_writedec(plen);
    terminal_write("\n");

    ip_send(dst_ip, IP_PROTO_UDP, buf, (uint16_t)(UDP_HDR_LEN + plen));
}

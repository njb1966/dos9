#include <icmp.h>
#include <ip.h>
#include <net.h>
#include <string.h>
#include <terminal.h>

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
    uint8_t  data[32];
} __attribute__((packed)) icmp_echo_t;

void icmp_rx(const void *pkt, uint16_t len, uint32_t src_ip) {
    if (len < 4) return;
    const icmp_echo_t *p = (const icmp_echo_t *)pkt;

    if (p->type == ICMP_ECHO_REQUEST) {
        /* Send echo reply. */
        icmp_echo_t r;
        r.type     = ICMP_ECHO_REPLY;
        r.code     = 0;
        r.checksum = 0;
        r.id       = p->id;
        r.seq      = p->seq;
        uint16_t copy = (uint16_t)((len - 8 < (uint16_t)sizeof(r.data)) ? len - 8 : (uint16_t)sizeof(r.data));
        memcpy(r.data, p->data, copy);
        r.checksum = ip_checksum(&r, (uint32_t)(8 + copy));
        ip_send(src_ip, IP_PROTO_ICMP, &r, (uint16_t)(8 + copy));

    } else if (p->type == ICMP_ECHO_REPLY) {
        terminal_write("[ICMP] pong from ");
        terminal_writedec((src_ip >> 24) & 0xFF); terminal_write(".");
        terminal_writedec((src_ip >> 16) & 0xFF); terminal_write(".");
        terminal_writedec((src_ip >>  8) & 0xFF); terminal_write(".");
        terminal_writedec( src_ip        & 0xFF);
        terminal_write(" seq="); terminal_writedec(ntohs(p->seq));
        terminal_write("\n");
    }
}

void icmp_ping(uint32_t dst_ip, uint16_t seq) {
    icmp_echo_t p;
    p.type     = ICMP_ECHO_REQUEST;
    p.code     = 0;
    p.checksum = 0;
    p.id       = htons(0xD09);
    p.seq      = htons(seq);
    memset(p.data, 0xAB, sizeof(p.data));
    p.checksum = ip_checksum(&p, sizeof(p));
    ip_send(dst_ip, IP_PROTO_ICMP, &p, sizeof(p));
}

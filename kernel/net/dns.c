#include <dns.h>
#include <udp.h>
#include <netif.h>
#include <net.h>
#include <process.h>
#include <string.h>

#define DNS_SERVER_PORT 53
#define DNS_LOCAL_PORT  1053     /* replies arrive here */

static uint16_t          g_xid       = 0;
static volatile int      g_dns_done  = 0;
static volatile uint32_t g_ip_result = 0;

typedef struct {
    const char *name;
    uint32_t     ip;
} dns_fallback_t;

static const dns_fallback_t g_fallbacks[] = {
    { "localhost",  IP4(127,0,0,1) },
    { "example.com", IP4(93,184,216,34) },
};

static int dns_fallback(const char *hostname, uint32_t *ip) {
    for (uint32_t i = 0; i < (uint32_t)(sizeof(g_fallbacks) / sizeof(g_fallbacks[0])); i++) {
        const char *a = hostname;
        const char *b = g_fallbacks[i].name;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') {
            *ip = g_fallbacks[i].ip;
            return 0;
        }
    }
    return -1;
}

/* Encode a hostname into DNS QNAME wire format.
   "www.example.com" → \x03www\x07example\x03com\x00
   Returns number of bytes written, or 0xFFFF on malformed/oversized input. */
static uint16_t encode_qname(const char *host, uint8_t *out, uint16_t outmax) {
    if (!host || !*host) return 0xFFFFu;
    uint16_t pos = 0;
    while (*host && (uint16_t)(pos + 2u) < outmax) {
        if (*host == '.') return 0xFFFFu;
        uint8_t *lenp = &out[pos++];
        uint8_t  len  = 0;
        while (*host && *host != '.' && pos < outmax) {
            if (len == 63u) return 0xFFFFu;
            out[pos++] = (uint8_t)*host++;
            len++;
        }
        *lenp = len;
        if (*host == '.') host++;
    }
    if (pos >= outmax) return 0xFFFFu;
    out[pos++] = 0;   /* root label */
    return pos;
}

/* Skip a DNS name in a packet, handling compression pointers (0xC0 xx).
   Returns pointer past the name, or NULL on malformed data. */
static const uint8_t *skip_name(const uint8_t *p, const uint8_t *end) {
    while (p < end) {
        uint8_t b = *p;
        if (b == 0)              return p + 1;
        if ((b & 0xC0) == 0xC0) return (p + 2 <= end) ? p + 2 : NULL;
        if (b > 63u) return NULL;
        if ((uint32_t)(end - p) < (uint32_t)(1u + b)) return NULL;
        p += 1u + (uint8_t)(b & 0x3Fu);
    }
    return NULL;
}

static void dns_rx(const void *payload, uint16_t len,
                   uint32_t src_ip, uint16_t src_port) {
    (void)src_ip; (void)src_port;
    if (len < 12u) return;

    const uint8_t *pkt = (const uint8_t *)payload;
    const uint8_t *end = pkt + len;

    uint16_t id      = (uint16_t)((uint16_t)pkt[0] << 8 | pkt[1]);
    uint16_t flags   = (uint16_t)((uint16_t)pkt[2] << 8 | pkt[3]);
    uint16_t qdcount = (uint16_t)((uint16_t)pkt[4] << 8 | pkt[5]);
    uint16_t ancount = (uint16_t)((uint16_t)pkt[6] << 8 | pkt[7]);

    if (id != g_xid) return;
    if (!(flags & 0x8000u)) return;
    if (flags & 0x000Fu) return;
    if (ancount == 0) return;

    const uint8_t *p = pkt + 12;

    /* Skip question section */
    for (uint16_t i = 0; i < qdcount && p; i++) {
        p = skip_name(p, end);
        if (!p || (uint32_t)(end - p) < 4u) return;
        p += 4;   /* QTYPE + QCLASS */
    }

    /* Parse answer section — return first A record */
    for (uint16_t i = 0; i < ancount && p && p < end; i++) {
        p = skip_name(p, end);
        if (!p || (uint32_t)(end - p) < 10u) break;

        uint16_t rtype = (uint16_t)((uint16_t)p[0] << 8 | p[1]);
        uint16_t rclass = (uint16_t)((uint16_t)p[2] << 8 | p[3]);
        /* p[4..7]=ttl */
        uint16_t rdlen = (uint16_t)((uint16_t)p[8] << 8 | p[9]);
        p += 10;

        if (rtype == 1u && rclass == 1u && rdlen == 4u && (uint32_t)(end - p) >= 4u) {
            g_ip_result = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                          ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
            g_dns_done = 1;
            return;
        }
        if ((uint32_t)(end - p) < (uint32_t)rdlen) break;
        p += rdlen;
    }
}

void dns_init(void) {
    udp_register(DNS_LOCAL_PORT, dns_rx);
}

int dns_lookup(const char *hostname, uint32_t *ip) {
    uint8_t  buf[512];
    uint16_t pos = 12;   /* leave room for header */

    uint16_t qname_len = encode_qname(hostname, buf + pos,
                                      (uint16_t)(sizeof(buf) - (size_t)pos - 4u));
    if (qname_len == 0xFFFFu) return -1;
    pos = (uint16_t)(pos + qname_len);
    if ((uint32_t)pos + 4u > sizeof(buf)) return -1;

    /* QTYPE A = 1, QCLASS IN = 1 */
    buf[pos++] = 0; buf[pos++] = 1;
    buf[pos++] = 0; buf[pos++] = 1;

    /* Advance XID, reset state, fill 12-byte header */
    if (!g_xid) g_xid = 1;
    g_xid++;
    g_dns_done  = 0;
    g_ip_result = 0;

    buf[0]  = (uint8_t)(g_xid >> 8); buf[1]  = (uint8_t)g_xid;
    buf[2]  = 0x01;                  buf[3]  = 0x00;   /* flags: RD=1 */
    buf[4]  = 0x00;                  buf[5]  = 0x01;   /* qdcount = 1 */
    buf[6]  = 0x00;                  buf[7]  = 0x00;   /* ancount = 0 */
    buf[8]  = 0x00;                  buf[9]  = 0x00;   /* nscount = 0 */
    buf[10] = 0x00;                  buf[11] = 0x00;   /* arcount = 0 */

    /* Retry loop: first send may be dropped if ARP is not yet resolved.
       Send every 50 ticks (~500 ms) until reply arrives or 6 tries. */
    if (!g_netif.dns) {
        return -1;
    }
    for (int retry = 0; retry < 6 && !g_dns_done; retry++) {
        udp_send(g_netif.dns, DNS_LOCAL_PORT, DNS_SERVER_PORT, buf, pos);
        for (int i = 0; i < 50 && !g_dns_done; i++)
            schedule();
    }

    if (!g_dns_done) {
        if (dns_fallback(hostname, ip) == 0) {
            return 0;
        }
        return -1;
    }
    *ip = g_ip_result;
    return 0;
}

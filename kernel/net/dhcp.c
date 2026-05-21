#include <dhcp.h>
#include <udp.h>
#include <netif.h>
#include <net.h>
#include <string.h>
#include <terminal.h>

#define DHCP_CLIENT_PORT  68
#define DHCP_SERVER_PORT  67
#define DHCP_XID          0x12D09001u   /* arbitrary transaction ID */

#define DHCP_MAGIC        0x63825363u

#define OPT_MSGTYPE     53
#define OPT_SERVERID    54
#define OPT_REQIP       50
#define OPT_SUBNET      1
#define OPT_GATEWAY     3
#define OPT_PARAMREQ    55
#define OPT_END         255

#define MSGTYPE_DISCOVER 1
#define MSGTYPE_OFFER    2
#define MSGTYPE_REQUEST  3
#define MSGTYPE_ACK      5

typedef struct {
    uint8_t  op, htype, hlen, hops;
    uint32_t xid;
    uint16_t secs, flags;
    uint32_t ciaddr, yiaddr, siaddr, giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;
    uint8_t  options[64];
} __attribute__((packed)) dhcp_pkt_t;

static int      g_done   = 0;
static uint32_t g_offer_ip;
static uint32_t g_server_ip;

static uint8_t *opt_append(uint8_t *p, uint8_t code, uint8_t len,
                            const uint8_t *data) {
    *p++ = code;
    *p++ = len;
    while (len--) *p++ = *data++;
    return p;
}

static void dhcp_send(uint8_t msgtype, uint32_t ciaddr,
                      uint32_t req_ip, uint32_t server_ip) {
    dhcp_pkt_t p;
    memset(&p, 0, sizeof(p));

    p.op    = 1;         /* BOOTREQUEST */
    p.htype = 1;         /* Ethernet */
    p.hlen  = 6;
    p.xid   = htonl(DHCP_XID);
    p.flags = htons(0x8000);  /* broadcast flag */
    p.ciaddr = htonl(ciaddr);
    memcpy(p.chaddr, g_netif.mac, 6);
    p.magic = htonl(DHCP_MAGIC);

    uint8_t *opt = p.options;
    uint8_t  mt  = msgtype;
    opt = opt_append(opt, OPT_MSGTYPE, 1, &mt);

    if (req_ip) {
        uint32_t rip = htonl(req_ip);
        opt = opt_append(opt, OPT_REQIP, 4, (uint8_t *)&rip);
    }
    if (server_ip) {
        uint32_t sip = htonl(server_ip);
        opt = opt_append(opt, OPT_SERVERID, 4, (uint8_t *)&sip);
    }
    if (msgtype == MSGTYPE_DISCOVER) {
        uint8_t params[] = { OPT_SUBNET, OPT_GATEWAY };
        opt = opt_append(opt, OPT_PARAMREQ, sizeof(params), params);
    }
    *opt++ = OPT_END;

    udp_send(0xFFFFFFFFu, DHCP_CLIENT_PORT, DHCP_SERVER_PORT,
             &p, sizeof(p));
}

static void dhcp_rx(const void *payload, uint16_t len,
                    uint32_t src_ip, uint16_t src_port) {
    (void)src_port;
    if (len < sizeof(dhcp_pkt_t)) return;
    const dhcp_pkt_t *p = (const dhcp_pkt_t *)payload;

    if (ntohl(p->xid) != DHCP_XID)    return;
    if (ntohl(p->magic) != DHCP_MAGIC) return;

    /* Parse options. */
    uint8_t msgtype = 0;
    uint32_t subnet  = 0, gateway = 0;
    uint32_t server  = src_ip;

    const uint8_t *opt = p->options;
    const uint8_t *end = p->options + 64;
    while (opt < end && *opt != OPT_END) {
        uint8_t code = *opt++;
        if (opt >= end) break;
        uint8_t olen = *opt++;
        if (opt + olen > end) break;

        switch (code) {
        case OPT_MSGTYPE:  msgtype = opt[0]; break;
        case OPT_SUBNET:
            subnet = ((uint32_t)opt[0]<<24)|((uint32_t)opt[1]<<16)
                    |((uint32_t)opt[2]<<8)|opt[3];
            break;
        case OPT_GATEWAY:
            gateway = ((uint32_t)opt[0]<<24)|((uint32_t)opt[1]<<16)
                     |((uint32_t)opt[2]<<8)|opt[3];
            break;
        case OPT_SERVERID:
            server = ((uint32_t)opt[0]<<24)|((uint32_t)opt[1]<<16)
                    |((uint32_t)opt[2]<<8)|opt[3];
            break;
        }
        opt += olen;
    }

    if (msgtype == MSGTYPE_OFFER) {
        g_offer_ip  = ntohl(p->yiaddr);
        g_server_ip = server;
        dhcp_send(MSGTYPE_REQUEST, 0, g_offer_ip, g_server_ip);

    } else if (msgtype == MSGTYPE_ACK) {
        g_netif.ip      = ntohl(p->yiaddr);
        g_netif.netmask = subnet  ? subnet  : IP4(255,255,255,0);
        g_netif.gateway = gateway ? gateway : IP4(10,0,2,2);
        g_netif.up      = 1;
        g_done          = 1;

        terminal_write("[DHCP] IP=");
        uint32_t ip = g_netif.ip;
        terminal_writedec((ip>>24)&0xFF); terminal_write(".");
        terminal_writedec((ip>>16)&0xFF); terminal_write(".");
        terminal_writedec((ip>> 8)&0xFF); terminal_write(".");
        terminal_writedec( ip     &0xFF); terminal_write("\n");
    }
}

void dhcp_start(void) {
    udp_register(DHCP_CLIENT_PORT, dhcp_rx);
    dhcp_send(MSGTYPE_DISCOVER, 0, 0, 0);
}

int dhcp_done(void) { return g_done; }

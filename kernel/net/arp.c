#include <arp.h>
#include <ethernet.h>
#include <netif.h>
#include <net.h>
#include <string.h>
#include <stddef.h>

#define ARP_TABLE_SZ  16
#define ARP_OP_REQ    1
#define ARP_OP_REPLY  2

typedef struct {
    uint16_t htype;   /* 1 = Ethernet */
    uint16_t ptype;   /* 0x0800 = IP */
    uint8_t  hlen;    /* 6 */
    uint8_t  plen;    /* 4 */
    uint16_t op;
    uint8_t  sha[6];  /* sender MAC */
    uint32_t spa;     /* sender IP  */
    uint8_t  tha[6];  /* target MAC */
    uint32_t tpa;     /* target IP  */
} __attribute__((packed)) arp_pkt_t;

typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    int      valid;
} arp_entry_t;

static arp_entry_t arp_table[ARP_TABLE_SZ];

void arp_add(uint32_t ip, const uint8_t *mac) {
    /* Update existing entry first. */
    for (int i = 0; i < ARP_TABLE_SZ; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            memcpy(arp_table[i].mac, mac, 6);
            return;
        }
    }
    /* Find empty slot. */
    for (int i = 0; i < ARP_TABLE_SZ; i++) {
        if (!arp_table[i].valid) {
            arp_table[i].ip    = ip;
            arp_table[i].valid = 1;
            memcpy(arp_table[i].mac, mac, 6);
            return;
        }
    }
    /* Evict slot 0 as a simple replacement policy. */
    arp_table[0].ip    = ip;
    arp_table[0].valid = 1;
    memcpy(arp_table[0].mac, mac, 6);
}

static void arp_send_request(uint32_t target_ip) {
    arp_pkt_t p;
    p.htype = htons(1);
    p.ptype = htons(0x0800);
    p.hlen  = 6;
    p.plen  = 4;
    p.op    = htons(ARP_OP_REQ);
    memcpy(p.sha, g_netif.mac, 6);
    p.spa   = htonl(g_netif.ip);
    memset(p.tha, 0, 6);
    p.tpa   = htonl(target_ip);
    ethernet_send((const uint8_t *)MAC_BCAST, ETH_TYPE_ARP, &p, sizeof(p));
}

void arp_rx(const void *pkt, uint16_t len) {
    if (len < sizeof(arp_pkt_t)) return;
    const arp_pkt_t *p = (const arp_pkt_t *)pkt;
    if (ntohs(p->htype) != 1 || ntohs(p->ptype) != 0x0800) return;

    uint32_t sender_ip = ntohl(p->spa);
    arp_add(sender_ip, p->sha);

    if (ntohs(p->op) == ARP_OP_REQ && ntohl(p->tpa) == g_netif.ip) {
        /* Reply to ARP requests for our own IP. */
        arp_pkt_t r;
        r.htype = htons(1);
        r.ptype = htons(0x0800);
        r.hlen  = 6;
        r.plen  = 4;
        r.op    = htons(ARP_OP_REPLY);
        memcpy(r.sha, g_netif.mac, 6);
        r.spa   = htonl(g_netif.ip);
        memcpy(r.tha, p->sha, 6);
        r.tpa   = p->spa;
        ethernet_send(p->sha, ETH_TYPE_ARP, &r, sizeof(r));
    }
}

const uint8_t *arp_resolve(uint32_t ip) {
    for (int i = 0; i < ARP_TABLE_SZ; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip)
            return arp_table[i].mac;
    }
    arp_send_request(ip);
    return NULL;
}

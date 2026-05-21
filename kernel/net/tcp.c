#include <tcp.h>
#include <ip.h>
#include <netif.h>
#include <net.h>
#include <process.h>
#include <string.h>
#include <stddef.h>

/* ── TCP header ──────────────────────────────────────────────────────────── */

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_off;   /* high nibble = data offset in 32-bit words */
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed)) tcp_hdr_t;

#define TCP_HDR_LEN  20

#define TF_FIN  0x01
#define TF_SYN  0x02
#define TF_RST  0x04
#define TF_PSH  0x08
#define TF_ACK  0x10
#define TF_URG  0x20

/* ── TCP pseudo-header (for checksum) ───────────────────────────────────── */

typedef struct {
    uint32_t src, dst;
    uint8_t  zero, proto;
    uint16_t tcp_len;
} __attribute__((packed)) tcp_pseudo_t;

/* ── Connection table ────────────────────────────────────────────────────── */

typedef struct {
    int      state;
    int      used;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint16_t local_port;
    uint32_t snd_nxt;     /* next seq to send */
    uint32_t snd_una;     /* oldest unacked seq */
    uint32_t rcv_nxt;     /* next expected remote seq */
    uint8_t  rxbuf[TCP_RX_BUF_SZ];
    uint16_t rxhead;      /* write index (filled by tcp_rx) */
    uint16_t rxtail;      /* read  index (drained by tcp_recv) */
    int      rx_eof;      /* remote sent FIN */
} tcp_conn_t;

static tcp_conn_t g_conns[TCP_MAX_CONNS];
static uint16_t   g_next_port = 49152;
static uint32_t   g_isn       = 0xA1B2C3D4u;

/* ── Checksum ────────────────────────────────────────────────────────────── */

static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                              const void *tcp_seg, uint16_t tcp_len) {
    tcp_pseudo_t ph;
    ph.src      = htonl(src_ip);
    ph.dst      = htonl(dst_ip);
    ph.zero     = 0;
    ph.proto    = IP_PROTO_TCP;
    ph.tcp_len  = htons(tcp_len);

    /* Compute over pseudo-header + TCP segment. */
    uint32_t sum = 0;
    const uint16_t *p = (const uint16_t *)&ph;
    for (uint32_t i = 0; i < sizeof(ph) / 2; i++) sum += p[i];

    p = (const uint16_t *)tcp_seg;
    uint32_t len = tcp_len;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8_t *)p;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/* ── Send a TCP segment ──────────────────────────────────────────────────── */

static void tcp_send_seg(tcp_conn_t *c, uint8_t flags,
                          const void *data, uint16_t dlen) {
    uint8_t buf[TCP_HDR_LEN + 1460];
    if ((uint32_t)(TCP_HDR_LEN + dlen) > sizeof(buf)) dlen = (uint16_t)(sizeof(buf) - TCP_HDR_LEN);

    tcp_hdr_t *th = (tcp_hdr_t *)buf;
    th->src_port = htons(c->local_port);
    th->dst_port = htons(c->remote_port);
    th->seq      = htonl(c->snd_nxt);
    th->ack      = (flags & TF_ACK) ? htonl(c->rcv_nxt) : 0;
    th->data_off = (TCP_HDR_LEN / 4) << 4;
    th->flags    = flags;
    th->window   = htons(TCP_RX_BUF_SZ);
    th->checksum = 0;
    th->urgent   = 0;

    if (data && dlen) memcpy(buf + TCP_HDR_LEN, data, dlen);

    uint16_t seg_len = (uint16_t)(TCP_HDR_LEN + dlen);
    th->checksum = tcp_checksum(g_netif.ip, c->remote_ip, buf, seg_len);

    ip_send(c->remote_ip, IP_PROTO_TCP, buf, seg_len);

    /* Advance snd_nxt for sequence-consuming flags. */
    if (flags & (TF_SYN | TF_FIN)) c->snd_nxt++;
    c->snd_nxt += dlen;
}

/* ── RX ring buffer helpers ──────────────────────────────────────────────── */

static uint16_t rxbuf_free(const tcp_conn_t *c) {
    return (uint16_t)(TCP_RX_BUF_SZ - 1
           - ((c->rxhead - c->rxtail + TCP_RX_BUF_SZ) % TCP_RX_BUF_SZ));
}
static uint16_t rxbuf_used(const tcp_conn_t *c) {
    return (uint16_t)((c->rxhead - c->rxtail + TCP_RX_BUF_SZ) % TCP_RX_BUF_SZ);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

int tcp_alloc(void) {
    for (int i = 0; i < TCP_MAX_CONNS; i++) {
        if (!g_conns[i].used) {
            g_conns[i].used   = 1;
            g_conns[i].state  = TCP_CLOSED;
            g_conns[i].rxhead = 0;
            g_conns[i].rxtail = 0;
            g_conns[i].rx_eof = 0;
            return i;
        }
    }
    return -1;
}

void tcp_free(int slot) {
    if (slot < 0 || slot >= TCP_MAX_CONNS) return;
    g_conns[slot].used  = 0;
    g_conns[slot].state = TCP_CLOSED;
}

int tcp_connect(int slot, uint32_t dst_ip, uint16_t dst_port) {
    if (slot < 0 || slot >= TCP_MAX_CONNS) return -1;
    tcp_conn_t *c = &g_conns[slot];

    c->remote_ip   = dst_ip;
    c->remote_port = dst_port;
    c->local_port  = g_next_port++;
    c->snd_nxt     = g_isn;
    g_isn         += 64000;        /* advance ISN between connections */
    c->snd_una     = c->snd_nxt;
    c->rcv_nxt     = 0;
    c->rxhead      = 0;
    c->rxtail      = 0;
    c->rx_eof      = 0;
    c->state       = TCP_SYN_SENT;

    tcp_send_seg(c, TF_SYN, NULL, 0);

    /* Wait for SYN-ACK (block via schedule). */
    int timeout = 500;   /* ~5 s at 100 Hz */
    while (c->state == TCP_SYN_SENT && timeout-- > 0)
        schedule();

    return (c->state == TCP_ESTABLISHED) ? 0 : -1;
}

int tcp_send(int slot, const void *buf, uint16_t len) {
    if (slot < 0 || slot >= TCP_MAX_CONNS) return -1;
    tcp_conn_t *c = &g_conns[slot];
    if (c->state != TCP_ESTABLISHED) return -1;
    if (len == 0) return 0;

    /* Chunk into max-segment-size pieces. */
    const uint8_t *p  = (const uint8_t *)buf;
    uint16_t       sent = 0;
    while (sent < len) {
        uint16_t chunk = len - sent;
        if (chunk > 1460) chunk = 1460;
        tcp_send_seg(c, TF_PSH | TF_ACK, p + sent, chunk);
        sent += chunk;
    }
    return (int)sent;
}

int tcp_recv(int slot, void *buf, uint16_t len) {
    if (slot < 0 || slot >= TCP_MAX_CONNS) return -1;
    tcp_conn_t *c = &g_conns[slot];

    /* Block until data or EOF. */
    while (rxbuf_used(c) == 0 && !c->rx_eof &&
           (c->state == TCP_ESTABLISHED || c->state == TCP_FIN_WAIT))
        schedule();

    if (rxbuf_used(c) == 0) return 0;   /* EOF */

    uint16_t avail = rxbuf_used(c);
    if (len > avail) len = avail;

    for (uint16_t i = 0; i < len; i++) {
        ((uint8_t *)buf)[i] = c->rxbuf[c->rxtail];
        c->rxtail = (uint16_t)((c->rxtail + 1) % TCP_RX_BUF_SZ);
    }
    return (int)len;
}

void tcp_close(int slot) {
    if (slot < 0 || slot >= TCP_MAX_CONNS) return;
    tcp_conn_t *c = &g_conns[slot];
    if (c->state == TCP_ESTABLISHED || c->state == TCP_CLOSE_WAIT) {
        c->state = TCP_FIN_WAIT;
        tcp_send_seg(c, TF_FIN | TF_ACK, NULL, 0);
    }
}

const char *tcp_state_str(int slot) {
    if (slot < 0 || slot >= TCP_MAX_CONNS || !g_conns[slot].used)
        return "closed";
    switch (g_conns[slot].state) {
    case TCP_CLOSED:      return "closed";
    case TCP_SYN_SENT:    return "connecting";
    case TCP_ESTABLISHED: return "established";
    case TCP_FIN_WAIT:    return "fin-wait";
    case TCP_CLOSE_WAIT:  return "close-wait";
    case TCP_TIME_WAIT:   return "time-wait";
    default:              return "unknown";
    }
}

/* ── Incoming segment handler ────────────────────────────────────────────── */

void tcp_rx(const void *pkt, uint16_t len, uint32_t src_ip, uint32_t dst_ip) {
    (void)dst_ip;
    if (len < TCP_HDR_LEN) return;
    const tcp_hdr_t *th  = (const tcp_hdr_t *)pkt;
    uint8_t          hdr_len = (uint8_t)((th->data_off >> 4) * 4);
    if (hdr_len < TCP_HDR_LEN || hdr_len > len) return;

    uint16_t       sport = ntohs(th->src_port);
    uint16_t       dport = ntohs(th->dst_port);
    uint32_t       seq   = ntohl(th->seq);
    uint32_t       ack   = ntohl(th->ack);
    const uint8_t *data  = (const uint8_t *)pkt + hdr_len;
    uint16_t       dlen  = (uint16_t)(len - hdr_len);

    /* Find matching connection. */
    tcp_conn_t *c = NULL;
    for (int i = 0; i < TCP_MAX_CONNS; i++) {
        if (!g_conns[i].used) continue;
        if (g_conns[i].remote_ip   == src_ip &&
            g_conns[i].remote_port == sport  &&
            g_conns[i].local_port  == dport) {
            c = &g_conns[i];
            break;
        }
    }
    if (!c) return;

    if (th->flags & TF_RST) {
        c->state = TCP_CLOSED;
        return;
    }

    switch (c->state) {
    case TCP_SYN_SENT:
        if ((th->flags & (TF_SYN | TF_ACK)) == (TF_SYN | TF_ACK)) {
            c->rcv_nxt = seq + 1;
            c->snd_una = ack;
            c->state   = TCP_ESTABLISHED;
            tcp_send_seg(c, TF_ACK, NULL, 0);
        }
        break;

    case TCP_ESTABLISHED:
    case TCP_FIN_WAIT:
        /* Update snd_una with received ACK. */
        if (th->flags & TF_ACK)
            c->snd_una = ack;

        /* Queue incoming data. */
        if (dlen > 0 && seq == c->rcv_nxt) {
            uint16_t space = rxbuf_free(c);
            uint16_t copy  = dlen < space ? dlen : space;
            for (uint16_t i = 0; i < copy; i++) {
                c->rxbuf[c->rxhead] = data[i];
                c->rxhead = (uint16_t)((c->rxhead + 1) % TCP_RX_BUF_SZ);
            }
            c->rcv_nxt += copy;
            tcp_send_seg(c, TF_ACK, NULL, 0);
        }

        /* Remote wants to close. */
        if (th->flags & TF_FIN) {
            c->rcv_nxt++;
            c->rx_eof = 1;
            tcp_send_seg(c, TF_ACK, NULL, 0);
            if (c->state == TCP_FIN_WAIT)
                c->state = TCP_TIME_WAIT;
            else
                c->state = TCP_CLOSE_WAIT;
        }
        break;

    case TCP_CLOSE_WAIT:
        if (th->flags & TF_ACK)
            c->snd_una = ack;
        break;

    default:
        break;
    }
}

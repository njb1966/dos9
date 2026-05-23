/*
 * netfs — Plan 9-style /net virtual filesystem.
 *
 * Layout:
 *   /net/info           read: "ip: A.B.C.D  mac: MM:MM:MM:MM:MM:MM  gw: G.G.G.G\n"
 *   /net/tcp/clone      open+read to allocate a connection slot
 *   /net/tcp/<n>/ctl    write "connect ip port\n" to establish TCP connection
 *   /net/tcp/<n>/data   read/write the TCP byte stream
 *   /net/tcp/<n>/status read current state string
 *
 * Usage from a user program:
 *   fd  = open("/net/tcp/clone", O_RDONLY);
 *   read(fd, buf, 4);   // e.g. "0\n"
 *   close(fd);
 *   ctl = open("/net/tcp/0/ctl", O_WRONLY);
 *   write(ctl, "connect 10.0.2.2 70\n", 20);
 *   close(ctl);
 *   dat = open("/net/tcp/0/data", O_RDWR);
 *   write(dat, "/\r\n", 3);
 *   while ((n = read(dat, buf, sizeof(buf))) > 0) { ... }
 *   close(dat);
 */

#include <netfs.h>
#include <vfs.h>
#include <tcp.h>
#include <dns.h>
#include <netif.h>
#include <net.h>
#include <process.h>
#include <kheap.h>
#include <string.h>
#include <terminal.h>

/* ── helpers ─────────────────────────────────────────────────────────────── */

static void copy_entry_name(char *dst, uint32_t nmax, const char *src) {
    uint32_t i = 0;
    if (!nmax) return;
    while (i + 1u < nmax && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int streq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int parse_uint(const char **pp, uint32_t *out) {
    const char *s = *pp;
    uint32_t v = 0;
    if (!*s) return -1;
    while (*s >= '0' && *s <= '9') {
        uint32_t digit = (uint32_t)(*s++ - '0');
        if (v > 429496729u || (v == 429496729u && digit > 5u))
            return -1;
        v = v * 10 + digit;
    }
    *out = v;
    *pp = s;
    return 0;
}

static int parse_ip(const char **pp, uint32_t *out) {
    const char *s = *pp;
    uint32_t oct[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
        if (*s < '0' || *s > '9') return -1;
        while (*s >= '0' && *s <= '9') {
            uint32_t digit = (uint32_t)(*s++ - '0');
            if (oct[i] > 25u || (oct[i] == 25u && digit > 5u))
                return -1;
            oct[i] = oct[i] * 10u + digit;
        }
        if (i < 3) {
            if (*s++ != '.') return -1;
        } else if (*s != '\0' && *s != '\n' && *s != ' ' && *s != '\r') {
            return -1;
        }
    }
    *pp = s;
    *out = IP4(oct[0], oct[1], oct[2], oct[3]);
    return 0;
}

/* ── /net/info ───────────────────────────────────────────────────────────── */

static int info_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    (void)v;
    char tmp[128];
    uint32_t ip  = g_netif.ip;
    uint32_t gw  = g_netif.gateway;
    /* Simple manual sprintf — our kernel lacks printf into buffers. */
    char *p = tmp;
    const char *s;

    s = "ip: ";      while (*s) *p++ = *s++;
    /* IP octets */
    uint8_t oct[4] = {
        (uint8_t)(ip>>24), (uint8_t)(ip>>16), (uint8_t)(ip>>8), (uint8_t)ip
    };
    for (int i = 0; i < 4; i++) {
        uint8_t n = oct[i];
        if (n >= 100) { *p++ = (char)('0' + n/100); n %= 100; *p++ = (char)('0' + n/10); n %= 10; }
        else if (n >= 10) { *p++ = (char)('0' + n/10); n %= 10; }
        *p++ = (char)('0' + n);
        if (i < 3) *p++ = '.';
    }
    s = "  mac: ";   while (*s) *p++ = *s++;
    const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        *p++ = hex[g_netif.mac[i] >> 4];
        *p++ = hex[g_netif.mac[i] & 0xF];
        if (i < 5) *p++ = ':';
    }
    s = "  gw: ";    while (*s) *p++ = *s++;
    uint8_t go[4] = {
        (uint8_t)(gw>>24), (uint8_t)(gw>>16), (uint8_t)(gw>>8), (uint8_t)gw
    };
    for (int i = 0; i < 4; i++) {
        uint8_t n = go[i];
        if (n >= 100) { *p++ = (char)('0' + n/100); n %= 100; *p++ = (char)('0' + n/10); n %= 10; }
        else if (n >= 10) { *p++ = (char)('0' + n/10); n %= 10; }
        *p++ = (char)('0' + n);
        if (i < 3) *p++ = '.';
    }
    *p++ = '\n';
    *p   = '\0';

    uint32_t total = (uint32_t)(p - tmp);
    if (off >= total) return 0;
    uint32_t copy = total - off;
    if (copy > len) copy = len;
    memcpy(buf, tmp + off, copy);
    return (int)copy;
}

static fs_ops_t info_ops = { .read = info_read };
static vnode_t  info_vnode = { .type = VTYPE_FILE, .ops = &info_ops };

/* ── /net/tcp/clone ──────────────────────────────────────────────────────── */

static fs_ops_t clone_ops;

typedef struct {
    int slot;
} clone_state_t;

static int clone_close(vnode_t *v) {
    if (v->priv) kfree(v->priv);
    kfree(v);
    return 0;
}

static int clone_open(vnode_t **vp, int flags) {
    (void)flags;
    int s = tcp_alloc();
    if (s < 0) return -1;

    vnode_t *v = (vnode_t *)kmalloc(sizeof(vnode_t));
    clone_state_t *st = (clone_state_t *)kmalloc(sizeof(clone_state_t));
    if (!v || !st) {
        if (v) kfree(v);
        if (st) kfree(st);
        return -1;
    }

    memset(v, 0, sizeof(vnode_t));
    st->slot = s;
    v->type = VTYPE_FILE;
    v->ops = &clone_ops;
    v->priv = st;
    *vp = v;
    return 0;
}

static int clone_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    if (off > 0) return 0;
    clone_state_t *st = (clone_state_t *)v->priv;
    char tmp[4];
    tmp[0] = (char)('0' + (st->slot % 10));
    tmp[1] = '\n';
    uint32_t copy = len < 2u ? len : 2u;
    memcpy(buf, tmp, copy);
    return (int)copy;
}

static fs_ops_t clone_ops = { .open = clone_open, .close = clone_close, .read = clone_read };
static vnode_t  clone_vnode = { .type = VTYPE_FILE, .ops = &clone_ops };

/* ── /net/tcp/<n>/ctl ────────────────────────────────────────────────────── */

static int ctl_write(vnode_t *v, const void *buf, uint32_t off, uint32_t len) {
    (void)off;
    int slot = (int)(uintptr_t)v->priv;
    char cmd[64];
    uint32_t copy = len < sizeof(cmd) - 1 ? len : sizeof(cmd) - 1;
    if (len >= sizeof(cmd)) return -1;
    memcpy(cmd, buf, copy);
    cmd[copy] = '\0';

    /* Parse "connect ip port" */
    const char *p = cmd;
    while (*p == ' ') p++;
    if (strncmp(p, "connect", 7) != 0) return -1;
    if (p[7] != '\0' && p[7] != ' ') return -1;
    p += 7;
    while (*p == ' ') p++;

    uint32_t ip = 0;
    if (parse_ip(&p, &ip) < 0) return -1;
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;

    uint32_t port = 0;
    if (parse_uint(&p, &port) < 0) return -1;
    if (port == 0 || port > 65535) return -1;
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;
    if (*p != '\0') return -1;

    int r = tcp_connect(slot, ip, (uint16_t)port);
    return r < 0 ? -1 : (int)len;
}

/* ── /net/tcp/<n>/data ───────────────────────────────────────────────────── */

static int data_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    (void)off;
    return tcp_recv((int)(uintptr_t)v->priv, buf, (uint16_t)len);
}

static int data_write(vnode_t *v, const void *buf, uint32_t off, uint32_t len) {
    (void)off;
    return tcp_send((int)(uintptr_t)v->priv, buf, (uint16_t)len);
}

/* ── /net/tcp/<n>/status ─────────────────────────────────────────────────── */

static int status_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    if (off > 0) return 0;
    const char *s = tcp_state_str((int)(uintptr_t)v->priv);
    uint32_t    slen = strlen(s);
    uint32_t    copy = slen < len ? slen : len;
    memcpy(buf, s, copy);
    return (int)copy;
}

/* ── Per-connection directory ────────────────────────────────────────────── */

static vnode_t ctl_vnodes[TCP_MAX_CONNS];
static vnode_t data_vnodes[TCP_MAX_CONNS];
static vnode_t status_vnodes[TCP_MAX_CONNS];
static vnode_t conn_vnodes[TCP_MAX_CONNS];

static fs_ops_t ctl_ops    = { .write = ctl_write };
static fs_ops_t data_ops   = { .read  = data_read, .write = data_write };
static fs_ops_t status_ops = { .read  = status_read };

static vnode_t *conn_lookup(vnode_t *d, const char *name) {
    int slot = (int)(uintptr_t)d->priv;
    if (streq(name, "ctl")) return &ctl_vnodes[slot];
    if (streq(name, "data")) return &data_vnodes[slot];
    if (streq(name, "status")) return &status_vnodes[slot];
    return NULL;
}

static int conn_readdir(vnode_t *d, uint32_t idx, char *name_out, uint32_t nmax) {
    (void)d;
    static const char *entries[] = { "ctl", "data", "status" };
    if (idx >= 3) return -1;
    copy_entry_name(name_out, nmax, entries[idx]);
    return 0;
}

static fs_ops_t conn_ops = { .lookup = conn_lookup, .readdir = conn_readdir };

/* ── /net/tcp directory ──────────────────────────────────────────────────── */

static vnode_t *tcp_dir_lookup(vnode_t *d, const char *name) {
    (void)d;
    if (streq(name, "clone")) return &clone_vnode;
    /* Parse connection number. */
    if (*name < '0' || *name > '9') return NULL;
    uint32_t n = 0;
    if (parse_uint(&name, &n) < 0) return NULL;
    if (*name != '\0') return NULL;
    if (n >= TCP_MAX_CONNS) return NULL;
    return &conn_vnodes[(int)n];
}

static int tcp_dir_readdir(vnode_t *d, uint32_t idx, char *name_out, uint32_t nmax) {
    (void)d;
    if (idx == 0) { copy_entry_name(name_out, nmax, "clone"); return 0; }
    /* Remaining entries are active connection numbers. */
    idx--;
    (void)idx; return -1;   /* keep it simple: just expose clone */
}

static fs_ops_t tcp_dir_ops = { .lookup = tcp_dir_lookup, .readdir = tcp_dir_readdir };
static vnode_t  tcp_dir_vnode = { .type = VTYPE_DIR, .ops = &tcp_dir_ops };

/* ── /net/resolve ────────────────────────────────────────────────────────── */

/* Write a hostname → blocking DNS lookup → read back "A.B.C.D\n" or "error\n".
   Separate open-write-close / open-read-close is the intended usage pattern. */

static char g_resolve_buf[MAX_PROCS][24];   /* per-process resolve mailbox */

static int resolve_slot(void) {
    process_t *self = process_current();
    if (!self) return -1;
    for (int i = 0; i < MAX_PROCS; i++) {
        if (process_get(i) == self) return i;
    }
    return -1;
}

static int resolve_write(vnode_t *v, const void *buf, uint32_t off, uint32_t len) {
    (void)off;
    (void)v;
    int slot = resolve_slot();
    if (slot < 0) return -1;
    char *out = g_resolve_buf[slot];
    char hostname[128];
    if (len >= (uint32_t)sizeof(hostname)) {
        memcpy(out, "error\n", 7);
        return (int)len;
    }
    uint32_t copy = len;
    memcpy(hostname, buf, copy);
    hostname[copy] = '\0';
    /* Strip trailing whitespace / newline */
    while (copy > 0 &&
           (hostname[copy-1u] == '\n' || hostname[copy-1u] == '\r' ||
            hostname[copy-1u] == ' '))
        hostname[--copy] = '\0';
    if (copy == 0) {
        memcpy(out, "error\n", 7);
        return (int)len;
    }

    uint32_t ip = 0;
    if (dns_lookup(hostname, &ip) == 0) {
        /* Format "A.B.C.D\n" into the per-process buffer. */
        char *p = out;
        uint8_t oct[4] = {
            (uint8_t)(ip >> 24), (uint8_t)(ip >> 16),
            (uint8_t)(ip >>  8), (uint8_t)ip
        };
        for (int i = 0; i < 4; i++) {
            uint8_t n = oct[i];
            if (n >= 100) { *p++ = (char)('0' + n/100); n = (uint8_t)(n % 100);
                            *p++ = (char)('0' + n/10);  n = (uint8_t)(n % 10); }
            else if (n >= 10) { *p++ = (char)('0' + n/10); n = (uint8_t)(n % 10); }
            *p++ = (char)('0' + n);
            if (i < 3) *p++ = '.';
        }
        *p++ = '\n'; *p = '\0';
    } else {
        memcpy(out, "error\n", 7);
    }
    return (int)len;
}

static int resolve_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    (void)v;
    int slot = resolve_slot();
    if (slot < 0) return -1;
    uint32_t total = (uint32_t)strlen(g_resolve_buf[slot]);
    if (off >= total) return 0;
    uint32_t copy = total - off;
    if (copy > len) copy = len;
    memcpy(buf, g_resolve_buf[slot] + off, copy);
    return (int)copy;
}

static fs_ops_t resolve_ops = { .read = resolve_read, .write = resolve_write };
static vnode_t  resolve_vnode = { .type = VTYPE_FILE, .ops = &resolve_ops };

/* ── /net root ───────────────────────────────────────────────────────────── */

static vnode_t *net_root_lookup(vnode_t *d, const char *name) {
    (void)d;
    if (streq(name, "info")) return &info_vnode;
    if (streq(name, "resolve")) return &resolve_vnode;
    if (streq(name, "tcp")) return &tcp_dir_vnode;
    return NULL;
}

static int net_root_readdir(vnode_t *d, uint32_t idx,
                             char *name_out, uint32_t nmax) {
    (void)d;
    if (idx == 0) { copy_entry_name(name_out, nmax, "info");    return 0; }
    if (idx == 1) { copy_entry_name(name_out, nmax, "resolve"); return 0; }
    if (idx == 2) { copy_entry_name(name_out, nmax, "tcp");     return 0; }
    return -1;
}

static fs_ops_t net_root_ops = { .lookup = net_root_lookup, .readdir = net_root_readdir };
static vnode_t  net_root_vnode = { .type = VTYPE_DIR, .ops = &net_root_ops };

/* ── init ─────────────────────────────────────────────────────────────────── */

void netfs_init(void) {
    for (int i = 0; i < TCP_MAX_CONNS; i++) {
        ctl_vnodes[i]    = (vnode_t){ .type = VTYPE_FILE, .ops = &ctl_ops,    .priv = (void *)(uintptr_t)i };
        data_vnodes[i]   = (vnode_t){ .type = VTYPE_FILE, .ops = &data_ops,   .priv = (void *)(uintptr_t)i };
        status_vnodes[i] = (vnode_t){ .type = VTYPE_FILE, .ops = &status_ops, .priv = (void *)(uintptr_t)i };
        conn_vnodes[i]   = (vnode_t){ .type = VTYPE_DIR,  .ops = &conn_ops,   .priv = (void *)(uintptr_t)i };
    }
    vfs_mount("/net", &net_root_vnode);
}

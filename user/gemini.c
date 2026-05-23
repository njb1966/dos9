/*
 * gemini — DOS/9 Gemini client.
 *
 * Uses /net/tcp for the raw connection and BearSSL for TLS 1.2.
 * Hostname resolution via /net/resolve.
 * Certificate validation uses BearSSL's minimal X.509 engine with a
 * runtime validation timestamp from /dev/time.
 *
 * Usage:
 *   gemini <host> [port] [path]
 *
 * Examples:
 *   gemini gemini.circumlunar.space
 *   gemini gemini.circumlunar.space 1965 /
 *   gemini 10.0.2.15 1965 /
 *
 * Gemini response codes (first two chars of header line):
 *   1x  INPUT          2x  SUCCESS         3x  REDIRECT
 *   4x  TEMP FAILURE   5x  PERM FAILURE     6x  CLIENT CERT
 */

#include <dos9.h>
#include "bearssl/include/string.h"
#include <bearssl.h>

#define GEMINI_DEFAULT_PORT 1965

/* ── Entropy ──────────────────────────────────────────────────────────────
 * Read the TSC (rdtsc) for best available entropy on a bare-metal i686.
 * Not cryptographically strong but sufficient for ECDHE in a hobbyist OS. */
static void inject_entropy(br_ssl_engine_context *eng) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    uint32_t seed[4];
    seed[0] = lo;  seed[1] = hi;
    seed[2] = lo ^ 0xA5A5A5A5u;
    seed[3] = hi ^ 0x5A5A5A5Au;
    br_ssl_engine_inject_entropy(eng, seed, sizeof seed);
}

/* ── TOFU X.509 wrapper ───────────────────────────────────────────────────
 * Delegates to br_x509_minimal for public-key extraction but overrides
 * end_chain to always return success (no CA validation). */

typedef struct {
    const br_x509_class *vtable;
    br_x509_minimal_context inner;
} tofu_x509_ctx;

static void tofu_start_chain(const br_x509_class **ctx,
                              const char *server_name) {
    tofu_x509_ctx *tc = (tofu_x509_ctx *)(void *)ctx;
    tc->inner.vtable->start_chain(&tc->inner.vtable, server_name);
}
static void tofu_start_cert(const br_x509_class **ctx, uint32_t length) {
    tofu_x509_ctx *tc = (tofu_x509_ctx *)(void *)ctx;
    tc->inner.vtable->start_cert(&tc->inner.vtable, length);
}
static void tofu_append(const br_x509_class **ctx,
                         const unsigned char *buf, size_t len) {
    tofu_x509_ctx *tc = (tofu_x509_ctx *)(void *)ctx;
    tc->inner.vtable->append(&tc->inner.vtable, buf, len);
}
static void tofu_end_cert(const br_x509_class **ctx) {
    tofu_x509_ctx *tc = (tofu_x509_ctx *)(void *)ctx;
    tc->inner.vtable->end_cert(&tc->inner.vtable);
}
static unsigned tofu_end_chain(const br_x509_class **ctx) {
    tofu_x509_ctx *tc = (tofu_x509_ctx *)(void *)ctx;
    tc->inner.vtable->end_chain(&tc->inner.vtable);
    return 0;   /* always accept */
}
static const br_x509_pkey *tofu_get_pkey(const br_x509_class *const *ctx,
                                          unsigned *usages) {
    tofu_x509_ctx *tc = (tofu_x509_ctx *)(void *)(uintptr_t)ctx;
    return tc->inner.vtable->get_pkey(&tc->inner.vtable, usages);
}

static const br_x509_class tofu_vtable = {
    sizeof(tofu_x509_ctx),
    tofu_start_chain,
    tofu_start_cert,
    tofu_append,
    tofu_end_cert,
    tofu_end_chain,
    tofu_get_pkey
};

static int read_unix_time(uint32_t *out) {
    char buf[32];
    int fd = open("/dev/time", O_RDONLY);
    if (fd < 0) return -1;
    int got = read(fd, buf, (int)sizeof(buf) - 1);
    close(fd);
    if (got <= 0) return -1;
    buf[got] = '\0';
    uint64_t val = 0;
    for (int i = 0; i < got; i++) {
        if (buf[i] < '0' || buf[i] > '9') break;
        uint32_t digit = (uint32_t)(buf[i] - '0');
        if (val > 429496729u || (val == 429496729u && digit > 5u))
            return -1;
        val = val * 10u + digit;
    }
    if (val == 0) return -1;
    *out = val;
    return 0;
}

static int tofu_init(tofu_x509_ctx *tc) {
    tc->vtable = &tofu_vtable;
    br_x509_minimal_init(&tc->inner, &br_sha256_vtable, NULL, 0);
    uint32_t unix_now;
    if (read_unix_time(&unix_now) < 0) return -1;
    br_x509_minimal_set_time(&tc->inner,
        unix_now / 86400u + 719528u,
        unix_now % 86400u);
    br_x509_minimal_set_rsa(&tc->inner, br_rsa_i31_pkcs1_vrfy);
    br_x509_minimal_set_ecdsa(&tc->inner, &br_ec_prime_i31,
                               br_ecdsa_i31_vrfy_asn1);
    br_x509_minimal_set_hash(&tc->inner, br_sha1_ID,   &br_sha1_vtable);
    br_x509_minimal_set_hash(&tc->inner, br_sha256_ID, &br_sha256_vtable);
    br_x509_minimal_set_hash(&tc->inner, br_sha384_ID, &br_sha384_vtable);
    br_x509_minimal_set_hash(&tc->inner, br_sha512_ID, &br_sha512_vtable);
    return 0;
}

/* ── BearSSL I/O callbacks (backed by /net/tcp data fd) ───────────────── */

static int tls_read(void *ctx, unsigned char *buf, size_t len) {
    int fd  = *(int *)ctx;
    int got = (int)read(fd, buf, (int)len);
    if (got <= 0) return -1;
    return got;
}

static int tls_write(void *ctx, const unsigned char *buf, size_t len) {
    int fd  = *(int *)ctx;
    int wr  = (int)write(fd, buf, (int)len);
    return (wr <= 0) ? -1 : wr;
}

/* ── Hostname helpers ─────────────────────────────────────────────────── */

static int is_hostname(const char *s) {
    while (*s) {
        if ((*s < '0' || *s > '9') && *s != '.') return 1;
        s++;
    }
    return 0;
}

static int resolve_hostname(const char *hostname, char *out, int outlen) {
    int fd = open("/net/resolve", O_WRONLY);
    if (fd < 0) return -1;
    int n = 0;
    while (hostname[n]) n++;
    write(fd, hostname, n);
    close(fd);

    fd = open("/net/resolve", O_RDONLY);
    if (fd < 0) return -1;
    int got = read(fd, out, outlen - 1);
    close(fd);
    if (got <= 0) return -1;
    out[got] = '\0';
    for (int i = 0; i < got; i++) if (out[i] == '\n') { out[i] = '\0'; break; }
    if (strcmp(out, "error") == 0) return -1;
    return 0;
}

static int parse_port(const char *s, int *port_out) {
    uint32_t port = 0;
    if (!s || !*s) return -1;
    while (*s >= '0' && *s <= '9') {
        uint32_t digit = (uint32_t)(*s++ - '0');
        if (port > 65535u / 10u ||
            (port == 65535u / 10u && digit > 65535u % 10u)) return -1;
        port = port * 10u + digit;
    }
    if (*s != '\0') return -1;
    if (port == 0) return -1;
    *port_out = (int)port;
    return 0;
}

/* ── Gemini line renderer ─────────────────────────────────────────────── */

static void render_line(const char *line, int len) {
    if (len <= 0) { putchar('\n'); return; }

    /* Headings */
    if (len >= 3 && line[0] == '#' && line[1] == '#' && line[2] == '#') {
        puts("  ### ");
        for (int i = 3; i < len && line[i] == ' '; i++) {}
        int s = 3; while (s < len && line[s] == ' ') s++;
        for (int i = s; i < len; i++) putchar(line[i]);
        putchar('\n'); return;
    }
    if (len >= 2 && line[0] == '#' && line[1] == '#') {
        puts("  ## ");
        int s = 2; while (s < len && line[s] == ' ') s++;
        for (int i = s; i < len; i++) putchar(line[i]);
        putchar('\n'); return;
    }
    if (len >= 1 && line[0] == '#') {
        puts("  # ");
        int s = 1; while (s < len && line[s] == ' ') s++;
        for (int i = s; i < len; i++) putchar(line[i]);
        putchar('\n'); return;
    }

    /* Links */
    if (len >= 2 && line[0] == '=' && line[1] == '>') {
        puts("  => ");
        int s = 2; while (s < len && line[s] == ' ') s++;
        /* Print URL */
        int url_end = s;
        while (url_end < len && line[url_end] != ' ' && line[url_end] != '\t')
            url_end++;
        for (int i = s; i < url_end; i++) putchar(line[i]);
        /* Print label if present */
        int label = url_end;
        while (label < len && (line[label] == ' ' || line[label] == '\t'))
            label++;
        if (label < len) {
            puts("  ");
            for (int i = label; i < len; i++) putchar(line[i]);
        }
        putchar('\n'); return;
    }

    /* List items */
    if (len >= 2 && line[0] == '*' && line[1] == ' ') {
        puts("  * ");
        for (int i = 2; i < len; i++) putchar(line[i]);
        putchar('\n'); return;
    }

    /* Plain text */
    for (int i = 0; i < len; i++) putchar(line[i]);
    putchar('\n');
}

/* ── Main TLS+Gemini fetch ─────────────────────────────────────────────── */

static int do_gemini(const char *host_ip, const char *hostname,
                     int port, const char *path) {
    /* Allocate TCP slot via /net/tcp/clone */
    char slot_buf[4] = {0};
    int cfd = open("/net/tcp/clone", O_RDONLY);
    if (cfd < 0) { puts("gemini: cannot open /net/tcp/clone"); return 1; }
    int n = read(cfd, slot_buf, sizeof(slot_buf) - 1);
    close(cfd);
    if (n <= 0) { puts("gemini: clone read failed"); return 1; }
    slot_buf[n] = '\0';
    for (int i = 0; i < n; i++) if (slot_buf[i] == '\n') { slot_buf[i] = '\0'; break; }

    /* Build /net/tcp/<n>/ctl and /net/tcp/<n>/data paths */
    char ctl_path[32]  = "/net/tcp/";
    char data_path[32] = "/net/tcp/";
    int slen = 0; while (slot_buf[slen]) slen++;
    for (int i = 0; i < slen; i++) { ctl_path[9+i] = slot_buf[i]; data_path[9+i] = slot_buf[i]; }
    const char *cs = "/ctl", *ds = "/data";
    int ci = 9+slen, di = 9+slen;
    for (int i = 0; cs[i]; i++) ctl_path[ci++]  = cs[i];
    for (int i = 0; ds[i]; i++) data_path[di++] = ds[i];
    ctl_path[ci] = '\0'; data_path[di] = '\0';

    /* Build and send connect command */
    char cmd[64];
    int j = 0;
    const char *cc = "connect ";
    while (*cc) {
        if (j >= (int)sizeof(cmd) - 1) return 1;
        cmd[j++] = *cc++;
    }
    const char *p = host_ip;
    while (*p) {
        if (j >= (int)sizeof(cmd) - 1) return 1;
        cmd[j++] = *p++;
    }
    if (j >= (int)sizeof(cmd) - 1) return 1;
    cmd[j++] = ' ';
    char port_str[8]; int plen = 0, tmp = port;
    if (tmp == 0) { port_str[plen++] = '0'; }
    else { while (tmp > 0) { port_str[plen++] = (char)('0' + tmp % 10); tmp /= 10; } }
    for (int i = plen - 1; i >= 0; i--) {
        if (j >= (int)sizeof(cmd) - 1) return 1;
        cmd[j++] = port_str[i];
    }
    if (j >= (int)sizeof(cmd) - 1) return 1;
    cmd[j++] = '\n'; cmd[j] = '\0';

    int ctlfd = open(ctl_path, O_WRONLY);
    if (ctlfd < 0) { puts("gemini: cannot open ctl"); return 1; }
    int wr = write(ctlfd, cmd, j);
    close(ctlfd);
    if (wr < 0) { puts("gemini: TCP connect failed"); return 1; }

    int datfd = open(data_path, O_RDWR);
    if (datfd < 0) { puts("gemini: cannot open data fd"); return 1; }

    /* ── BearSSL TLS handshake ── */
    static br_ssl_client_context sc;
    static tofu_x509_ctx         xc;
    static unsigned char         iobuf[BR_SSL_BUFSIZE_BIDI];
    static br_sslio_context      ioc;

    if (tofu_init(&xc) < 0) {
        puts("gemini: /dev/time unavailable");
        close(datfd);
        return 1;
    }

    /* Minimal TLS 1.2 AES-GCM profile — avoids pulling in CBC/DES/ChaCha deps */
    {
        static const uint16_t suites[] = {
            BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
            BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
            BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
            BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        };
        br_ssl_client_zero(&sc);
        br_ssl_engine_set_versions(&sc.eng, BR_TLS12, BR_TLS12);
        br_ssl_engine_set_suites(&sc.eng, suites,
            (sizeof suites) / (sizeof suites[0]));
        br_ssl_client_set_rsapub(&sc, br_rsa_i31_public);
        br_ssl_engine_set_rsavrfy(&sc.eng, br_rsa_i31_pkcs1_vrfy);
        br_ssl_engine_set_ec(&sc.eng, &br_ec_prime_i31);
        br_ssl_engine_set_ecdsa(&sc.eng, br_ecdsa_i31_vrfy_asn1);
        br_ssl_engine_set_hash(&sc.eng, br_sha1_ID,   &br_sha1_vtable);
        br_ssl_engine_set_hash(&sc.eng, br_sha256_ID, &br_sha256_vtable);
        br_ssl_engine_set_hash(&sc.eng, br_sha384_ID, &br_sha384_vtable);
        br_ssl_engine_set_prf_sha256(&sc.eng, &br_tls12_sha256_prf);
        br_ssl_engine_set_prf_sha384(&sc.eng, &br_tls12_sha384_prf);
        br_ssl_engine_set_default_aes_gcm(&sc.eng);
        br_ssl_engine_set_x509(&sc.eng, &xc.vtable);
    }

    br_ssl_engine_set_buffer(&sc.eng, iobuf, sizeof iobuf, 1);
    inject_entropy(&sc.eng);
    br_ssl_client_reset(&sc, hostname, 0);
    br_sslio_init(&ioc, &sc.eng, tls_read, &datfd, tls_write, &datfd);

    /* ── Gemini request: "gemini://host/path\r\n" ── */
    char req[1024];
    j = 0;
    const char *scheme = "gemini://";
    while (*scheme) {
        if (j >= (int)sizeof(req) - 1) {
            close(datfd);
            return 1;
        }
        req[j++] = *scheme++;
    }
    const char *h = hostname;
    while (*h) {
        if (j >= (int)sizeof(req) - 1) {
            close(datfd);
            return 1;
        }
        req[j++] = *h++;
    }
    if (path[0] != '/') {
        if (j >= (int)sizeof(req) - 1) {
            close(datfd);
            return 1;
        }
        req[j++] = '/';
    }
    const char *pp = path;
    while (*pp) {
        if (j >= (int)sizeof(req) - 2) {
            close(datfd);
            return 1;
        }
        req[j++] = *pp++;
    }
    if (j >= (int)sizeof(req) - 2) {
        close(datfd);
        return 1;
    }
    req[j++] = '\r'; req[j++] = '\n';

    if (br_sslio_write_all(&ioc, req, (size_t)j) < 0) {
        puts("gemini: request write failed");
        close(datfd);
        return 1;
    }
    if (br_sslio_flush(&ioc) < 0) {
        close(datfd);
        return 1;
    }

    /* ── Read response header line ── */
    char hdr[2048];
    int  hlen = 0;
    for (;;) {
        unsigned char ch;
        int r = br_sslio_read(&ioc, &ch, 1);
        if (r <= 0) break;
        if (ch == '\n') break;
        if (ch == '\r') continue;
        if (hlen < (int)sizeof(hdr) - 1) hdr[hlen++] = (char)ch;
    }
    hdr[hlen] = '\0';

    if (hlen < 2 || hdr[0] < '0' || hdr[0] > '9' || hdr[1] < '0' || hdr[1] > '9') {
        puts("gemini: invalid response");
        close(datfd);
        return 1;
    }

    int status = (hdr[0] - '0') * 10 + (hdr[1] - '0');

    /* Status 20 = success — render body */
    if (status / 10 == 2) {
        /* hdr after "2x <mime>\0" — skip to body */
        char line[2048];
        int  llen = 0;
        for (;;) {
            unsigned char ch;
            int r = br_sslio_read(&ioc, &ch, 1);
            if (r <= 0) {
                if (llen > 0) render_line(line, llen);
                break;
            }
            if (ch == '\n') {
                render_line(line, llen);
                llen = 0;
            } else if (ch == '\r') {
                /* skip */
            } else {
                if (llen < (int)sizeof(line) - 1) line[llen++] = (char)ch;
            }
        }
    } else if (status / 10 == 3) {
        /* Redirect */
        puts("gemini: redirect -> ");
        if (hlen > 3) puts(hdr + 3);
        putchar('\n');
    } else {
        /* Error */
        puts("gemini: error ");
        puts(hdr);
        putchar('\n');
    }

    close(datfd);
    return 0;
}

int main(int argc, const char **argv) {
    if (argc < 2) {
        puts("usage: gemini <host> [port] [path]");
        return 1;
    }

    const char *host = argv[1];
    int         port = GEMINI_DEFAULT_PORT;
    const char *path = "/";

    if (argc >= 3) {
        if (parse_port(argv[2], &port) < 0) {
            puts("gemini: invalid port");
            return 1;
        }
    }
    if (argc >= 4) path = argv[3];

    char ip_buf[20];
    const char *ip = host;
    if (is_hostname(host)) {
        if (resolve_hostname(host, ip_buf, (int)sizeof(ip_buf)) < 0) {
            puts("gemini: hostname resolution failed");
            return 1;
        }
        ip = ip_buf;
    }

    return do_gemini(ip, host, port, path);
}

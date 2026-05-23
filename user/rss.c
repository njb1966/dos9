/*
 * rss — DOS/9 RSS reader over plain HTTP.
 *
 * Usage:
 *   rss <host> [port] [path]
 *
 * Examples:
 *   rss example.com
 *   rss 10.0.2.100 1234 /
 *
 * This is intentionally small: it issues an HTTP/1.0 GET over /net/tcp,
 * reads the full response, and prints feed/item titles plus links from RSS
 * and Atom feeds.
 */

#include <dos9.h>

#define RSS_DEFAULT_PORT 80
#define RSS_RECV_MAX   (256u * 1024u)

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
    int hlen = 0;
    while (hostname[hlen]) hlen++;
    write(fd, hostname, hlen);
    close(fd);

    fd = open("/net/resolve", O_RDONLY);
    if (fd < 0) return -1;
    int n = read(fd, out, outlen - 1);
    close(fd);
    if (n <= 0) return -1;
    out[n] = '\0';
    for (int i = 0; i < n; i++) if (out[i] == '\n') { out[i] = '\0'; break; }
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

static int parse_content_length(const char *resp, const char *hdr_end,
                                uint32_t *len_out) {
    const char *p = resp;
    while (p < hdr_end) {
        const char *line = p;
        while (p < hdr_end && *p != '\n') p++;
        const char *line_end = p;
        if (p < hdr_end && *p == '\n') p++;
        if (line_end > line && line_end[-1] == '\r') line_end--;

        const char *q = line;
        while (q < line_end && (*q == ' ' || *q == '\t')) q++;
        if ((line_end - q) >= 15 && strncmp(q, "Content-Length:", 15) == 0) {
            q += 15;
            while (q < line_end && (*q == ' ' || *q == '\t')) q++;
            uint32_t len = 0;
            if (q >= line_end) return -1;
            while (q < line_end) {
                if (*q < '0' || *q > '9') return -1;
                uint32_t digit = (uint32_t)(*q - '0');
                if (len > 429496729u || (len == 429496729u && digit > 5u))
                    return -1;
                len = len * 10u + digit;
                q++;
            }
            *len_out = len;
            return 0;
        }
    }
    return -1;
}

static int tcp_connect(const char *host_ip, int port) {
    char slot_buf[4] = {0};
    int cfd = open("/net/tcp/clone", O_RDONLY);
    if (cfd < 0) return -1;
    int n = read(cfd, slot_buf, sizeof(slot_buf) - 1);
    close(cfd);
    if (n <= 0) return -1;
    slot_buf[n] = '\0';
    for (int i = 0; i < n; i++) if (slot_buf[i] == '\n') { slot_buf[i] = '\0'; break; }

    char ctl_path[32]  = "/net/tcp/";
    char data_path[32] = "/net/tcp/";
    int slen = 0;
    while (slot_buf[slen]) slen++;
    if (9 + slen + 4 >= (int)sizeof(ctl_path)) return -1;
    for (int i = 0; i < slen; i++) {
        ctl_path[9 + i]  = slot_buf[i];
        data_path[9 + i] = slot_buf[i];
    }
    ctl_path[9 + slen]  = '\0';
    data_path[9 + slen] = '\0';
    const char *ctl_sfx  = "/ctl";
    const char *data_sfx = "/data";
    int ci = 9 + slen, di = 9 + slen;
    for (int i = 0; ctl_sfx[i]; i++) ctl_path[ci++] = ctl_sfx[i];
    for (int i = 0; data_sfx[i]; i++) data_path[di++] = data_sfx[i];
    ctl_path[ci] = '\0';
    data_path[di] = '\0';

    char cmd[64];
    int j = 0;
    const char *cc = "connect ";
    while (*cc) {
        if (j >= (int)sizeof(cmd) - 1) return -1;
        cmd[j++] = *cc++;
    }
    const char *p = host_ip;
    while (*p) {
        if (j >= (int)sizeof(cmd) - 1) return -1;
        cmd[j++] = *p++;
    }
    if (j >= (int)sizeof(cmd) - 3) return -1;
    cmd[j++] = ' ';

    char port_str[8];
    int plen = 0;
    int tmp = port;
    if (tmp == 0) port_str[plen++] = '0';
    else {
        while (tmp > 0) {
            port_str[plen++] = (char)('0' + tmp % 10);
            tmp /= 10;
        }
    }
    for (int i = plen - 1; i >= 0; i--) {
        if (j >= (int)sizeof(cmd) - 1) return -1;
        cmd[j++] = port_str[i];
    }
    if (j >= (int)sizeof(cmd) - 1) return -1;
    cmd[j++] = '\n';
    cmd[j] = '\0';

    int ctlfd = open(ctl_path, O_WRONLY);
    if (ctlfd < 0) return -1;
    int wr = write(ctlfd, cmd, j);
    close(ctlfd);
    if (wr < 0) return -1;

    int datfd = open(data_path, O_RDWR);
    if (datfd < 0) return -1;
    return datfd;
}

static int http_fetch(const char *host_ip, const char *host_header,
                      int port, const char *path,
                      char **resp_out, uint32_t *resp_len_out) {
    int fd = tcp_connect(host_ip, port);
    if (fd < 0) return -1;

    char req[512];
    int j = 0;
    const char *meth = "GET ";
    while (*meth) {
        if (j >= (int)sizeof(req) - 1) { close(fd); return -1; }
        req[j++] = *meth++;
    }
    if (!path || !*path) path = "/";
    if (*path != '/') {
        if (j >= (int)sizeof(req) - 1) { close(fd); return -1; }
        req[j++] = '/';
    }
    while (*path) {
        if (j >= (int)sizeof(req) - 1) { close(fd); return -1; }
        req[j++] = *path++;
    }
    const char *mid = " HTTP/1.0\r\nHost: ";
    while (*mid) {
        if (j >= (int)sizeof(req) - 1) { close(fd); return -1; }
        req[j++] = *mid++;
    }
    while (*host_header) {
        if (j >= (int)sizeof(req) - 1) { close(fd); return -1; }
        req[j++] = *host_header++;
    }
    const char *tail =
        "\r\nUser-Agent: DOS9/rss\r\n"
        "Accept: application/rss+xml, application/atom+xml, text/xml, */*\r\n"
        "Connection: close\r\n\r\n";
    while (*tail) {
        if (j >= (int)sizeof(req) - 1) { close(fd); return -1; }
        req[j++] = *tail++;
    }

    if (write(fd, req, j) < 0) {
        close(fd);
        return -1;
    }

    uint32_t cap = 8192u;
    uint32_t total = 0;
    char *resp = malloc(cap);
    if (!resp) { close(fd); return -1; }
    uint32_t hdr_end_off = 0;
    uint32_t body_len = 0;
    int have_hdr = 0;
    int have_len = 0;

    for (;;) {
        if (total == cap) {
            if (cap >= RSS_RECV_MAX / 2u) {
                free(resp);
                close(fd);
                return -1;
            }
            uint32_t ncap = cap * 2u;
            char *nr = realloc(resp, ncap);
            if (!nr) {
                free(resp);
                close(fd);
                return -1;
            }
            resp = nr;
            cap = ncap;
        }

        int n = read(fd, resp + total, (int)(cap - total));
        if (n < 0) {
            free(resp);
            close(fd);
            return -1;
        }
        if (n == 0) break;
        total += (uint32_t)n;

        if (!have_hdr) {
            for (uint32_t i = 0; i + 3 < total; i++) {
                if (resp[i] == '\r' && resp[i + 1] == '\n' &&
                    resp[i + 2] == '\r' && resp[i + 3] == '\n') {
                    hdr_end_off = i + 4;
                    have_hdr = 1;
                    break;
                }
            }
            if (!have_hdr) {
                for (uint32_t i = 0; i + 1 < total; i++) {
                    if (resp[i] == '\n' && resp[i + 1] == '\n') {
                        hdr_end_off = i + 2;
                        have_hdr = 1;
                        break;
                    }
                }
            }
            if (have_hdr && parse_content_length(resp, resp + hdr_end_off, &body_len) == 0)
                have_len = 1;
        }

        if (have_hdr && have_len && total >= hdr_end_off + body_len) break;
    }

    close(fd);
    *resp_out = resp;
    *resp_len_out = total;
    return 0;
}

static int tag_boundary(char c) {
    return c == '>' || c == '/' || c == ' ' || c == '\t' ||
           c == '\r' || c == '\n' || c == '\0';
}

static const char *find_open_tag(const char *p, const char *end, const char *name) {
    int nlen = (int)strlen(name);
    while (p < end) {
        if (*p == '<' && p + 1 < end && p[1] != '/') {
            const char *q = p + 1;
            int i = 0;
            while (i < nlen && q + i < end && q[i] == name[i]) i++;
            if (i == nlen && q + nlen < end && tag_boundary(q[nlen])) return p;
        }
        p++;
    }
    return NULL;
}

static const char *find_close_tag(const char *p, const char *end, const char *name) {
    int nlen = (int)strlen(name);
    while (p < end) {
        if (*p == '<' && p + 2 < end && p[1] == '/') {
            const char *q = p + 2;
            int i = 0;
            while (i < nlen && q + i < end && q[i] == name[i]) i++;
            if (i == nlen && q + nlen < end && tag_boundary(q[nlen])) return p;
        }
        p++;
    }
    return NULL;
}

static void trim_ws(char *s) {
    int start = 0;
    while (s[start] == ' ' || s[start] == '\t' ||
           s[start] == '\r' || s[start] == '\n')
        start++;
    int end = start;
    while (s[end]) end++;
    while (end > start &&
           (s[end - 1] == ' ' || s[end - 1] == '\t' ||
            s[end - 1] == '\r' || s[end - 1] == '\n'))
        end--;
    if (start > 0) {
        int i = 0;
        while (start < end) s[i++] = s[start++];
        s[i] = '\0';
    } else {
        s[end] = '\0';
    }
}

static void copy_entity(char *dst, int *di, int dmax, const char *entity) {
    if (*di >= dmax - 1) return;
    dst[(*di)++] = entity[0];
}

static void copy_xml_text(char *dst, int dmax, const char *p, const char *end) {
    int di = 0;
    while (p < end && *p) {
        if (*p == '<') break;
        if (*p == '&') {
            if ((end - p) >= 5 && strncmp(p, "&amp;", 5) == 0) {
                copy_entity(dst, &di, dmax, "&");
                p += 5;
                continue;
            }
            if ((end - p) >= 4 && strncmp(p, "&lt;", 4) == 0) {
                copy_entity(dst, &di, dmax, "<");
                p += 4;
                continue;
            }
            if ((end - p) >= 4 && strncmp(p, "&gt;", 4) == 0) {
                copy_entity(dst, &di, dmax, ">");
                p += 4;
                continue;
            }
            if ((end - p) >= 6 && strncmp(p, "&quot;", 6) == 0) {
                copy_entity(dst, &di, dmax, "\"");
                p += 6;
                continue;
            }
            if ((end - p) >= 6 && strncmp(p, "&apos;", 6) == 0) {
                copy_entity(dst, &di, dmax, "'");
                p += 6;
                continue;
            }
        }
        if (di < dmax - 1) dst[di++] = *p;
        p++;
    }
    dst[di] = '\0';
    trim_ws(dst);
}

static int extract_tag_text(const char *start, const char *end,
                            const char *name, char *out, int outmax) {
    const char *open = find_open_tag(start, end, name);
    if (!open) return -1;
    const char *gt = open;
    while (gt < end && *gt != '>') gt++;
    if (gt >= end) return -1;
    const char *close = find_close_tag(gt + 1, end, name);
    if (!close) return -1;
    copy_xml_text(out, outmax, gt + 1, close);
    return 0;
}

static int extract_link_href(const char *start, const char *end,
                             char *out, int outmax) {
    const char *p = start;
    while ((p = find_open_tag(p, end, "link")) != NULL) {
        const char *gt = p;
        while (gt < end && *gt != '>') gt++;
        if (gt >= end) return -1;

        const char *a = p + 1;
        while (a < gt) {
            while (a < gt && (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n'))
                a++;
            if (a >= gt || *a == '>') break;
            const char *name = a;
            while (a < gt && *a != '=' && *a != ' ' && *a != '\t' &&
                   *a != '\r' && *a != '\n' && *a != '>' && *a != '/')
                a++;
            int nlen = (int)(a - name);
            while (a < gt && (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n'))
                a++;
            if (a >= gt || *a != '=') {
                while (a < gt && *a != ' ' && *a != '\t' &&
                       *a != '\r' && *a != '\n' && *a != '>')
                    a++;
                continue;
            }
            a++;
            while (a < gt && (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n'))
                a++;
            char quote = 0;
            if (*a == '"' || *a == '\'') quote = *a++;
            const char *v = a;
            while (a < gt && ((quote && *a != quote) ||
                              (!quote && *a != ' ' && *a != '\t' &&
                               *a != '\r' && *a != '\n' && *a != '>' && *a != '/')))
                a++;
            if (nlen == 4 && strncmp(name, "href", 4) == 0) {
                int len = (int)(a - v);
                if (len >= outmax) len = outmax - 1;
                memcpy(out, v, (size_t)len);
                out[len] = '\0';
                trim_ws(out);
                return 0;
            }
            if (quote && a < gt && *a == quote) a++;
        }
        p = gt + 1;
    }
    return -1;
}

static void print_item(uint32_t idx, const char *title, const char *link) {
    printf("[%u] ", idx);
    if (title[0]) puts(title);
    else puts("(untitled)");
    if (link[0]) {
        printf("    %s\n", link);
    }
}

static void render_feed(const char *body, uint32_t len) {
    const char *p = body;
    const char *end = body + len;
    uint32_t first_item = len;
    const char *item = find_open_tag(p, end, "item");
    const char *entry = find_open_tag(p, end, "entry");
    if (item && (!entry || item < entry)) first_item = (uint32_t)(item - body);
    else if (entry) first_item = (uint32_t)(entry - body);

    char feed_title[256];
    feed_title[0] = '\0';
    if (extract_tag_text(body, body + first_item, "title", feed_title, (int)sizeof(feed_title)) == 0) {
        printf("feed: %s\n", feed_title);
    }

    uint32_t count = 0;
    p = body;
    while (p < end) {
        const char *open = find_open_tag(p, end, "item");
        const char *kind = "item";
        if (!open) {
            open = find_open_tag(p, end, "entry");
            kind = "entry";
        }
        if (!open) break;

        const char *gt = open;
        while (gt < end && *gt != '>') gt++;
        if (gt >= end) break;
        const char *close = find_close_tag(gt + 1, end, kind);
        if (!close) close = end;

        char title[256]; title[0] = '\0';
        char link[256];  link[0]  = '\0';

        extract_tag_text(gt + 1, close, "title", title, (int)sizeof(title));
        if (extract_tag_text(gt + 1, close, "link", link, (int)sizeof(link)) < 0) {
            extract_link_href(gt + 1, close, link, (int)sizeof(link));
        }

        count++;
        print_item(count, title, link);
        p = close + 1;
    }

    if (count == 0) puts("rss: no items found");
}

static void usage(void) {
    puts("usage: rss <host> [port] [path]");
    puts("  fetch an RSS/Atom feed over plain HTTP and print its entries");
}

int main(int argc, const char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    const char *host = argv[1];
    int port = RSS_DEFAULT_PORT;
    const char *path = "/";

    if (argc >= 3) {
        if (parse_port(argv[2], &port) < 0) {
            puts("rss: invalid port");
            return 1;
        }
    }
    if (argc >= 4) path = argv[3];

    char ip_buf[20];
    const char *ip = host;
    if (is_hostname(host)) {
        if (resolve_hostname(host, ip_buf, (int)sizeof(ip_buf)) < 0) {
            puts("rss: hostname resolution failed");
            return 1;
        }
        ip = ip_buf;
    }

    char *resp = NULL;
    uint32_t resp_len = 0;
    if (http_fetch(ip, host, port, path, &resp, &resp_len) < 0) {
        puts("rss: fetch failed");
        return 1;
    }

    const char *body = resp;
    const char *end = resp + resp_len;
    const char *hdr_end = NULL;
    for (const char *p = resp; p + 3 < end; p++) {
        if (p[0] == '\r' && p[1] == '\n' && p[2] == '\r' && p[3] == '\n') {
            hdr_end = p + 4;
            break;
        }
    }
    if (!hdr_end) {
        for (const char *p = resp; p + 1 < end; p++) {
            if (p[0] == '\n' && p[1] == '\n') {
                hdr_end = p + 2;
                break;
            }
        }
    }
    if (hdr_end) body = hdr_end;

    if (resp_len >= 12 && strncmp(resp, "HTTP/", 5) == 0) {
        const char *p = resp;
        while (p < end && *p != ' ') p++;
        if (p < end) {
            p++;
            if (p + 2 < end && p[0] >= '0' && p[0] <= '9' &&
                p[1] >= '0' && p[1] <= '9' && p[2] >= '0' && p[2] <= '9') {
                int code = (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
                if (code < 200 || code >= 300) {
                    printf("rss: HTTP %d\n", code);
                    free(resp);
                    return 1;
                }
            }
        }
    }

    render_feed(body, (uint32_t)(end - body));
    free(resp);
    return 0;
}

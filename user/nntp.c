/*
 * nntp — DOS/9 NNTP client.
 *
 * Usage:
 *   nntp <host> [port] [group] [article]
 *
 * The client resolves hostnames via /net/resolve, opens a TCP connection
 * through /net/tcp, issues a small reader handshake, and prints the server
 * transcript verbatim.
 */

#include <dos9.h>

#define NNTP_DEFAULT_PORT 119

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

static int send_all(int fd, const char *buf, int len) {
    int off = 0;
    while (off < len) {
        int wr = write(fd, buf + off, len - off);
        if (wr <= 0) return -1;
        off += wr;
    }
    return 0;
}

static int append_char(char *buf, int *len, int max, char c) {
    if (*len >= max - 1) return -1;
    buf[(*len)++] = c;
    return 0;
}

static int append_str(char *buf, int *len, int max, const char *s) {
    while (*s) {
        if (append_char(buf, len, max, *s++) < 0) return -1;
    }
    return 0;
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

    char ctl_path[32] = "/net/tcp/";
    char data_path[32] = "/net/tcp/";
    int slen = 0;
    while (slot_buf[slen]) slen++;
    if (9 + slen + 5 >= (int)sizeof(ctl_path)) return -1;
    for (int i = 0; i < slen; i++) {
        ctl_path[9 + i] = slot_buf[i];
        data_path[9 + i] = slot_buf[i];
    }
    ctl_path[9 + slen] = '\0';
    data_path[9 + slen] = '\0';

    const char *ctl_sfx = "/ctl";
    const char *data_sfx = "/data";
    int ci = 9 + slen;
    int di = 9 + slen;
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
    if (send_all(ctlfd, cmd, j) < 0) {
        close(ctlfd);
        return -1;
    }
    close(ctlfd);

    int datfd = open(data_path, O_RDWR);
    if (datfd < 0) return -1;
    return datfd;
}

static int nntp_session(const char *host_ip, int port,
                        const char *group, const char *article) {
    int fd = tcp_connect(host_ip, port);
    if (fd < 0) return -1;

    char line[256];
    int len = 0;
    if (append_str(line, &len, (int)sizeof(line), "MODE READER\r\n") < 0 ||
        send_all(fd, line, len) < 0) {
        close(fd);
        return -1;
    }

    if (group && *group) {
        len = 0;
        if (append_str(line, &len, (int)sizeof(line), "GROUP ") < 0 ||
            append_str(line, &len, (int)sizeof(line), group) < 0 ||
            append_str(line, &len, (int)sizeof(line), "\r\n") < 0 ||
            send_all(fd, line, len) < 0) {
            close(fd);
            return -1;
        }
    }

    if (article && *article) {
        len = 0;
        if (append_str(line, &len, (int)sizeof(line), "ARTICLE ") < 0 ||
            append_str(line, &len, (int)sizeof(line), article) < 0 ||
            append_str(line, &len, (int)sizeof(line), "\r\n") < 0 ||
            send_all(fd, line, len) < 0) {
            close(fd);
            return -1;
        }
    }

    len = 0;
    if (append_str(line, &len, (int)sizeof(line), "QUIT\r\n") < 0 ||
        send_all(fd, line, len) < 0) {
        close(fd);
        return -1;
    }

    char buf[256];
    int n;
    while ((n = read(fd, buf, (int)sizeof(buf))) > 0) {
        if (write(1, buf, n) < 0) {
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}

static void usage(void) {
    puts("usage: nntp <host> [port] [group] [article]");
    puts("  connect to an NNTP server and print the transcript");
}

int main(int argc, const char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    const char *host = argv[1];
    int port = NNTP_DEFAULT_PORT;
    const char *group = "";
    const char *article = "";

    if (argc >= 3) {
        if (parse_port(argv[2], &port) < 0) {
            puts("nntp: invalid port");
            return 1;
        }
    }
    if (argc >= 4) group = argv[3];
    if (argc >= 5) article = argv[4];

    char ip_buf[20];
    const char *ip = host;
    if (is_hostname(host)) {
        if (resolve_hostname(host, ip_buf, (int)sizeof(ip_buf)) < 0) {
            puts("nntp: hostname resolution failed");
            return 1;
        }
        ip = ip_buf;
    }

    if (nntp_session(ip, port, group, article) < 0) {
        puts("nntp: session failed");
        return 1;
    }

    return 0;
}

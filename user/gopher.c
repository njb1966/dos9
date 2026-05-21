/*
 * gopher — DOS/9 Gopher client.
 *
 * Uses the Plan 9-style /net/tcp filesystem to establish TCP connections.
 *
 * Usage:
 *   gopher <ip> [port] [selector]
 *
 * Examples:
 *   gopher 10.0.2.2 70 /             -- browse root menu
 *   gopher 10.0.2.15 70              -- default selector is empty string
 *
 * Gopher item types (RFC 1436):
 *   0  text file       1  directory menu   7  search
 *   h  HTML (escaped)  i  info line
 *
 * Note: DNS is not yet implemented; use IP addresses.
 */

#include <dos9.h>

#define GOPHER_DEFAULT_PORT 70

/* Print a Gopher menu line, stripping the item type prefix. */
static void print_menu_line(const char *line, int len) {
    if (len < 1) return;
    char type  = line[0];
    /* Info lines (type 'i') are informational; print without prefix. */
    /* Other lines: "TYPE<tab>name<tab>selector<tab>host<tab>port" */
    if (type == 'i') {
        /* Print display string (field 1). */
        const char *p = line + 1;
        while (*p && *p != '\t' && (int)(p - line) < len) {
            putchar(*p++);
        }
        putchar('\n');
        return;
    }
    /* Show type + display name for navigable items. */
    if (type == '1') puts("[DIR] ");
    else if (type == '0') puts("[TXT] ");
    else if (type == '7') puts("[SRH] ");
    else if (type == 'h') puts("[WEB] ");
    else { putchar('['); putchar(type); puts("]   "); }

    /* Print display string. */
    const char *p = line + 1;
    while (*p && *p != '\t' && (int)(p - line) < len) {
        putchar(*p++);
    }
    putchar('\n');
}

static int do_gopher(const char *host_ip, int port, const char *selector) {
    /* Step 1: Allocate a TCP connection via /net/tcp/clone. */
    char clone_num[4] = {0};
    int  cfd = open("/net/tcp/clone", O_RDONLY);
    if (cfd < 0) {
        puts("gopher: cannot open /net/tcp/clone");
        return 1;
    }
    int n = read(cfd, clone_num, sizeof(clone_num) - 1);
    close(cfd);
    if (n <= 0) {
        puts("gopher: clone read failed");
        return 1;
    }
    clone_num[n] = '\0';
    /* Strip newline. */
    for (int i = 0; i < n; i++) if (clone_num[i] == '\n') { clone_num[i] = '\0'; break; }

    /* Step 2: Build ctl and data paths. */
    char ctl_path[32]  = "/net/tcp/";
    char data_path[32] = "/net/tcp/";
    /* Append slot number and file name manually. */
    int slen = 0;
    while (clone_num[slen]) slen++;
    for (int i = 0; i < slen; i++) {
        ctl_path[9 + i]  = clone_num[i];
        data_path[9 + i] = clone_num[i];
    }
    ctl_path[9 + slen]  = '\0';
    data_path[9 + slen] = '\0';
    /* Append "/ctl" and "/data". */
    const char *ctl_sfx  = "/ctl";
    const char *data_sfx = "/data";
    int ci = 9 + slen, di = 9 + slen;
    for (int i = 0; ctl_sfx[i];  i++) ctl_path[ci++]  = ctl_sfx[i];
    for (int i = 0; data_sfx[i]; i++) data_path[di++] = data_sfx[i];
    ctl_path[ci]  = '\0';
    data_path[di] = '\0';

    /* Step 3: Build "connect ip port\n" command. */
    char cmd[64];
    /* Manual: "connect " + ip + " " + port + "\n" */
    int j = 0;
    const char *cc = "connect ";
    while (*cc) cmd[j++] = *cc++;
    const char *p = host_ip;
    while (*p) cmd[j++] = *p++;
    cmd[j++] = ' ';
    /* Print port as decimal. */
    char port_str[8];
    int plen = 0;
    int tmp = port;
    if (tmp == 0) { port_str[plen++] = '0'; }
    else { while (tmp > 0) { port_str[plen++] = (char)('0' + tmp % 10); tmp /= 10; } }
    for (int i = plen - 1; i >= 0; i--) cmd[j++] = port_str[i];
    cmd[j++] = '\n'; cmd[j] = '\0';

    /* Step 4: Write connect command to ctl. */
    int ctlfd = open(ctl_path, O_WRONLY);
    if (ctlfd < 0) {
        puts("gopher: cannot open ctl");
        return 1;
    }
    int wr = write(ctlfd, cmd, j);
    close(ctlfd);
    if (wr < 0) {
        puts("gopher: connect failed");
        return 1;
    }

    /* Step 5: Open data fd, send Gopher selector. */
    int datfd = open(data_path, O_RDWR);
    if (datfd < 0) {
        puts("gopher: cannot open data fd");
        return 1;
    }

    /* Send selector + CRLF. */
    int sellen = 0;
    while (selector[sellen]) sellen++;
    write(datfd, selector, sellen);
    write(datfd, "\r\n", 2);

    /* Step 6: Read and display response. */
    char buf[256];
    char line[256];
    int  llen = 0;

    while ((n = read(datfd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                line[llen] = '\0';
                if (llen == 1 && line[0] == '.') break;  /* end-of-menu */
                print_menu_line(line, llen);
                llen = 0;
            } else {
                if (llen < (int)sizeof(line) - 1)
                    line[llen++] = c;
            }
        }
    }

    close(datfd);
    return 0;
}

int main(int argc, const char **argv) {
    if (argc < 2) {
        puts("usage: gopher <ip> [port] [selector]");
        return 1;
    }

    const char *ip       = argv[1];
    int         port     = GOPHER_DEFAULT_PORT;
    const char *selector = "";

    if (argc >= 3) {
        /* Parse port manually. */
        const char *ps = argv[2];
        port = 0;
        while (*ps >= '0' && *ps <= '9') { port = port * 10 + (*ps++ - '0'); }
    }
    if (argc >= 4) selector = argv[3];

    return do_gopher(ip, port, selector);
}

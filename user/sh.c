#include <dos9.h>

#define LINE_MAX    128
#define PATH_MAX     64
#define DIRENT_MAX   64
#define HIST_SIZE    32

/* ── history ─────────────────────────────────────────────────────────────── */

static char hist_buf[HIST_SIZE][LINE_MAX];
static int  hist_count = 0;
static int  hist_next  = 0;   /* next write slot (circular) */

static void hist_add(const char *line) {
    if (!line[0]) return;
    /* Skip exact duplicate of the most recent entry. */
    if (hist_count > 0) {
        int prev = (hist_next - 1 + HIST_SIZE) % HIST_SIZE;
        if (strcmp(hist_buf[prev], line) == 0) return;
    }
    strcpy(hist_buf[hist_next], line);
    hist_next = (hist_next + 1) % HIST_SIZE;
    if (hist_count < HIST_SIZE) hist_count++;
}

/* pos: 0 = oldest entry, hist_count-1 = newest entry. */
static const char *hist_get(int pos) {
    if (pos < 0 || pos >= hist_count) return NULL;
    int idx = (hist_next - hist_count + pos + HIST_SIZE * 2) % HIST_SIZE;
    return hist_buf[idx];
}

/* ── terminal helpers ────────────────────────────────────────────────────── */

/*
 * Our VGA terminal's '\b' moves left AND clears the cell, so sending n
 * backspaces erases n characters and parks the cursor at the start.
 */
static void erase_chars(int n) {
    for (int i = 0; i < n; i++)
        write(STDOUT_FILENO, "\b", 1);
}

/* ── tab completion ──────────────────────────────────────────────────────── */

static void complete(char *buf, int *lenp, int max) {
    buf[*lenp] = '\0';

    /* Find start of the last word in the buffer. */
    int ws = *lenp;
    while (ws > 0 && buf[ws - 1] != ' ') ws--;
    char *word    = buf + ws;
    int   wordlen = *lenp - ws;

    /* Split word at the last '/': dir part + prefix after the slash. */
    char dir[PATH_MAX];
    int  plen      = 0;
    int  last_slash = -1;
    for (int i = 0; i < wordlen; i++)
        if (word[i] == '/') last_slash = i;

    if (last_slash >= 0) {
        int dlen = last_slash + 1;
        if (dlen >= PATH_MAX) return;
        memcpy(dir, word, (size_t)dlen);
        dir[dlen] = '\0';
        plen = wordlen - dlen;
    } else {
        /* No slash: complete against root (useful for /disk/ listings). */
        dir[0] = '/'; dir[1] = '\0';
        plen = wordlen;
    }
    char *prefix = word + (wordlen - plen);

    int fd = open(dir, O_RDONLY);
    if (fd < 0) return;

    char matches[8][DIRENT_MAX];
    int  nm = 0;
    char entry[DIRENT_MAX];
    for (uint32_t idx = 0;
         readdir(fd, idx, entry, sizeof(entry)) == 0;
         idx++) {
        if (strncmp(entry, prefix, (size_t)plen) == 0) {
            if (nm < 8) strcpy(matches[nm], entry);
            nm++;
        }
    }
    close(fd);

    if (nm == 0) return;

    if (nm == 1) {
        /* Unique match — append the suffix directly. */
        int mlen = (int)strlen(matches[0]);
        for (int i = plen; i < mlen && *lenp < max - 1; i++) {
            char ch = matches[0][i];
            buf[(*lenp)++] = ch;
            write(STDOUT_FILENO, &ch, 1);
        }
        return;
    }

    /* Multiple matches — list them, then redraw the prompt + current input. */
    write(STDOUT_FILENO, "\n", 1);
    for (int i = 0; i < nm && i < 8; i++) {
        write(STDOUT_FILENO, matches[i], strlen(matches[i]));
        write(STDOUT_FILENO, "  ", 2);
    }
    if (nm > 8) write(STDOUT_FILENO, "...", 3);
    write(STDOUT_FILENO, "\n", 1);
    write(STDOUT_FILENO, "sh> ", 4);
    buf[*lenp] = '\0';
    write(STDOUT_FILENO, buf, (size_t)*lenp);
}

/* ── line input ──────────────────────────────────────────────────────────── */

static int readline(char *buf, int max) {
    int  len      = 0;
    int  hist_pos = hist_count;   /* past-end index = "current unsaved line" */
    char saved[LINE_MAX];
    saved[0] = '\0';
    buf[0]   = '\0';

    for (;;) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) break;

        if (c == '\r' || c == '\n') {
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        /* ESC [ x — ANSI cursor key sequence from the keyboard driver. */
        if (c == '\x1b') {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (seq[0] != '[')                        continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

            if (seq[1] != 'A' && seq[1] != 'B') continue;  /* ignore left/right */

            int target;
            if (seq[1] == 'A') {   /* up — older entry */
                if (hist_pos == 0) continue;
                if (hist_pos == hist_count) {
                    buf[len] = '\0';
                    strcpy(saved, buf);
                }
                target = hist_pos - 1;
            } else {               /* down — newer entry */
                if (hist_pos == hist_count) continue;
                target = hist_pos + 1;
            }

            const char *h = (target == hist_count) ? saved : hist_get(target);
            if (!h) continue;

            erase_chars(len);
            len = (int)strlen(h);
            if (len >= max) len = max - 1;
            strcpy(buf, h);
            write(STDOUT_FILENO, buf, (size_t)len);
            hist_pos = target;
            continue;
        }

        /* Tab — path completion. */
        if (c == '\t') {
            buf[len] = '\0';
            complete(buf, &len, max);
            hist_pos = hist_count;   /* reset history position after completion */
            continue;
        }

        /* Backspace / DEL. */
        if ((c == '\b' || c == 127) && len > 0) {
            len--;
            write(STDOUT_FILENO, "\b", 1);
            continue;
        }

        if (c < 32) continue;

        if (len < max - 1) {
            buf[len++] = c;
            write(STDOUT_FILENO, &c, 1);
        }
    }

    buf[len] = '\0';
    return len;
}

/* ── argument splitting ──────────────────────────────────────────────────── */

static char *split_arg(char *buf) {
    while (*buf && *buf != ' ') buf++;
    if (!*buf) return NULL;
    *buf = '\0';
    return buf + 1;
}

/* ── built-in commands ───────────────────────────────────────────────────── */

static void cmd_help(void) {
    puts("Commands:");
    puts("  help            this message");
    puts("  echo <text>     print text");
    puts("  ls [path]       list directory");
    puts("  cat <path>      print file");
    puts("  exec <path>     spawn program (async)");
    puts("  run  <path>     spawn and wait");
    puts("  rm   <path>     remove file");
    puts("  pid             print our PID");
    puts("Keys:");
    puts("  Up/Down         history navigation");
    puts("  Tab             path completion");
}

static void cmd_echo(const char *arg) {
    if (arg) puts(arg);
    else     write(STDOUT_FILENO, "\n", 1);
}

static void cmd_ls(const char *path) {
    if (!path || !*path) path = "/";
    int fd = open(path, O_RDONLY);
    if (fd < 0) { puts("ls: open failed"); return; }

    char name[DIRENT_MAX];
    uint32_t idx = 0;
    while (readdir(fd, idx, name, sizeof(name)) == 0) {
        puts(name);
        idx++;
    }
    close(fd);
}

static void cmd_cat(const char *path) {
    if (!path || !*path) { puts("cat: missing path"); return; }
    int fd = open(path, O_RDONLY);
    if (fd < 0) { puts("cat: open failed"); return; }

    char buf[128];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, (size_t)n);
    close(fd);
}

static void cmd_exec(const char *path) {
    if (!path || !*path) { puts("exec: missing path"); return; }
    int pid = exec(path);
    if (pid < 0) puts("exec: failed");
    else         printf("  pid %d\n", pid);
}

static void cmd_run(const char *path) {
    if (!path || !*path) { puts("run: missing path"); return; }
    int pid = exec(path);
    if (pid < 0) { puts("run: exec failed"); return; }
    waitpid(pid);
}

static void cmd_rm(const char *path) {
    if (!path || !*path) { puts("rm: missing path"); return; }
    if (unlink(path) < 0) puts("rm: failed");
}

static void cmd_pid(void) {
    printf("  pid: %d\n", getpid());
}

/* ── main loop ───────────────────────────────────────────────────────────── */

int main(void) {
    puts("DOS/9 sh - type 'help' for commands");

    char line[LINE_MAX];
    for (;;) {
        write(STDOUT_FILENO, "sh> ", 4);
        if (readline(line, sizeof(line)) == 0) continue;
        hist_add(line);   /* before split_arg modifies line in-place */

        char *arg = split_arg(line);
        const char *cmd = line;

        if      (strcmp(cmd, "help") == 0) cmd_help();
        else if (strcmp(cmd, "echo") == 0) cmd_echo(arg);
        else if (strcmp(cmd, "ls")   == 0) cmd_ls(arg);
        else if (strcmp(cmd, "cat")  == 0) cmd_cat(arg);
        else if (strcmp(cmd, "exec") == 0) cmd_exec(arg);
        else if (strcmp(cmd, "run")  == 0) cmd_run(arg);
        else if (strcmp(cmd, "rm")   == 0) cmd_rm(arg);
        else if (strcmp(cmd, "pid")  == 0) cmd_pid();
        else puts("sh: unknown command");
    }
}

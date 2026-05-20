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
    if (hist_count > 0) {
        int prev = (hist_next - 1 + HIST_SIZE) % HIST_SIZE;
        if (strcmp(hist_buf[prev], line) == 0) return;
    }
    strcpy(hist_buf[hist_next], line);
    hist_next = (hist_next + 1) % HIST_SIZE;
    if (hist_count < HIST_SIZE) hist_count++;
}

/* pos: 0 = oldest entry, hist_count-1 = newest. */
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

    int ws = *lenp;
    while (ws > 0 && buf[ws - 1] != ' ') ws--;
    char *word    = buf + ws;
    int   wordlen = *lenp - ws;

    char dir[PATH_MAX];
    int  plen       = 0;
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
        int mlen = (int)strlen(matches[0]);
        for (int i = plen; i < mlen && *lenp < max - 1; i++) {
            char ch = matches[0][i];
            buf[(*lenp)++] = ch;
            write(STDOUT_FILENO, &ch, 1);
        }
        return;
    }

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
    int  hist_pos = hist_count;
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

        if (c == '\x1b') {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (seq[0] != '[')                        continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

            if (seq[1] != 'A' && seq[1] != 'B') continue;

            int target;
            if (seq[1] == 'A') {
                if (hist_pos == 0) continue;
                if (hist_pos == hist_count) {
                    buf[len] = '\0';
                    strcpy(saved, buf);
                }
                target = hist_pos - 1;
            } else {
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

        if (c == '\t') {
            buf[len] = '\0';
            complete(buf, &len, max);
            hist_pos = hist_count;
            continue;
        }

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

/* ── pipeline ────────────────────────────────────────────────────────────── */

static void execute_pipeline(char *left, char *right) {
    while (*left  == ' ') left++;
    while (*right == ' ') right++;
    int ll = (int)strlen(left);
    int rl = (int)strlen(right);
    while (ll > 0 && left[ll-1]  == ' ') left[--ll]  = '\0';
    while (rl > 0 && right[rl-1] == ' ') right[--rl] = '\0';

    if (!left[0] || !right[0]) { puts("pipe: empty command"); return; }

    int p[2];
    if (pipe(p) < 0) { puts("pipe: failed"); return; }

    int saved_out = dup(STDOUT_FILENO);
    int saved_in  = dup(STDIN_FILENO);
    if (saved_out < 0 || saved_in < 0) {
        puts("pipe: dup failed");
        close(p[0]); close(p[1]);
        if (saved_out >= 0) close(saved_out);
        if (saved_in  >= 0) close(saved_in);
        return;
    }

    /* Left side: stdout -> pipe write. */
    dup2(p[1], STDOUT_FILENO);
    int left_pid = exec(left);
    dup2(saved_out, STDOUT_FILENO);
    close(saved_out);
    close(p[1]);   /* shell drops write ref — only left child holds it now */

    /* Right side: stdin <- pipe read. */
    dup2(p[0], STDIN_FILENO);
    int right_pid = exec(right);
    dup2(saved_in, STDIN_FILENO);
    close(saved_in);
    close(p[0]);

    if (left_pid  > 0) waitpid(left_pid);
    if (right_pid > 0) waitpid(right_pid);
}

/* ── built-in commands ───────────────────────────────────────────────────── */

/* Forward declaration — cmd_help is defined after the command table. */
static void cmd_help(const char *arg);

#define IS_HELP(a) ((a) && strcmp((a), "--help") == 0)

static void cmd_echo(const char *arg) {
    if (IS_HELP(arg)) {
        puts("usage: echo <text>");
        puts("  write text to stdout followed by a newline");
        return;
    }
    if (arg) puts(arg);
    else     write(STDOUT_FILENO, "\n", 1);
}

static void cmd_ls(const char *path) {
    if (IS_HELP(path)) {
        puts("usage: ls [path]");
        puts("  list directory entries (default path: /)");
        return;
    }
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
    if (IS_HELP(path)) {
        puts("usage: cat <path>");
        puts("  print file to stdout");
        return;
    }
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
    if (IS_HELP(path)) {
        puts("usage: exec <path>");
        puts("  spawn a program and return immediately (async)");
        return;
    }
    if (!path || !*path) { puts("exec: missing path"); return; }
    int pid = exec(path);
    if (pid < 0) puts("exec: failed");
    else         printf("  pid %d\n", pid);
}

static void cmd_run(const char *path) {
    if (IS_HELP(path)) {
        puts("usage: run <path>");
        puts("  spawn a program and wait for it to exit");
        return;
    }
    if (!path || !*path) { puts("run: missing path"); return; }
    int pid = exec(path);
    if (pid < 0) { puts("run: exec failed"); return; }
    waitpid(pid);
}

static void cmd_rm(const char *path) {
    if (IS_HELP(path)) {
        puts("usage: rm <path>");
        puts("  remove a file; rm /proc/<pid> kills a process");
        return;
    }
    if (!path || !*path) { puts("rm: missing path"); return; }
    if (unlink(path) < 0) puts("rm: failed");
}

static void cmd_pid(const char *arg) {
    if (IS_HELP(arg)) {
        puts("usage: pid");
        puts("  print the process ID of this shell");
        return;
    }
    printf("  pid: %d\n", getpid());
}

/* ── command table ───────────────────────────────────────────────────────── */

typedef void (*cmd_fn_t)(const char *);

typedef struct {
    const char *name;
    cmd_fn_t    fn;
    const char *usage;
    const char *desc;
} cmd_entry_t;

static const cmd_entry_t cmds[] = {
    { "help", cmd_help, "help [cmd]",    "list commands, or describe one"  },
    { "echo", cmd_echo, "echo <text>",   "write text to stdout"            },
    { "ls",   cmd_ls,   "ls [path]",     "list directory"                  },
    { "cat",  cmd_cat,  "cat <path>",    "print file to stdout"            },
    { "exec", cmd_exec, "exec <path>",   "spawn program (no wait)"         },
    { "run",  cmd_run,  "run <path>",    "spawn and wait for exit"         },
    { "rm",   cmd_rm,   "rm <path>",     "remove file (or kill /proc/pid)" },
    { "pid",  cmd_pid,  "pid",           "print our process ID"            },
    { NULL,   NULL,     NULL,            NULL                              },
};

/* ── help ────────────────────────────────────────────────────────────────── */

static void cmd_help(const char *arg) {
    if (arg && *arg) {
        if (strcmp(arg, "--help") == 0) {
            puts("usage: help [cmd]");
            puts("  list all built-in commands, or describe one");
            return;
        }
        for (int i = 0; cmds[i].name; i++) {
            if (strcmp(cmds[i].name, arg) == 0) {
                /* usage line then description */
                puts(cmds[i].usage);
                puts(cmds[i].desc);
                return;
            }
        }
        puts("sh: help: unknown command");
        return;
    }

    puts("Commands:");
    for (int i = 0; cmds[i].name; i++) {
        write(STDOUT_FILENO, "  ", 2);
        int ulen = (int)strlen(cmds[i].usage);
        write(STDOUT_FILENO, cmds[i].usage, (size_t)ulen);
        /* pad usage column to 16 chars */
        for (int p = ulen; p < 16; p++) write(STDOUT_FILENO, " ", 1);
        puts(cmds[i].desc);
    }
    puts("Keys:");
    puts("  Up/Down         history navigation");
    puts("  Tab             path completion");
    puts("Syntax:");
    puts("  left | right    pipeline (external programs only)");
}

/* ── main loop ───────────────────────────────────────────────────────────── */

int main(void) {
    puts("DOS/9 sh - type 'help' for commands");

    char line[LINE_MAX];
    for (;;) {
        write(STDOUT_FILENO, "sh> ", 4);
        if (readline(line, sizeof(line)) == 0) continue;
        hist_add(line);

        /* Check for pipeline before modifying line. */
        char *pipe_pos = NULL;
        for (int i = 0; line[i]; i++) {
            if (line[i] == '|') { pipe_pos = &line[i]; break; }
        }
        if (pipe_pos) {
            *pipe_pos = '\0';
            execute_pipeline(line, pipe_pos + 1);
            continue;
        }

        char *arg = split_arg(line);
        const char *cmd = line;

        int found = 0;
        for (int i = 0; cmds[i].name; i++) {
            if (strcmp(cmd, cmds[i].name) == 0) {
                cmds[i].fn(arg);
                found = 1;
                break;
            }
        }
        if (!found) puts("sh: unknown command");
    }
}

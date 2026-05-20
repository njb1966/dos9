#include <dos9.h>

#define LINE_MAX    128
#define PATH_MAX     64
#define DIRENT_MAX   64
#define HIST_SIZE    32

/* ── history ─────────────────────────────────────────────────────────────── */

static char hist_buf[HIST_SIZE][LINE_MAX];
static int  hist_count = 0;
static int  hist_next  = 0;

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

static const char *hist_get(int pos) {
    if (pos < 0 || pos >= hist_count) return NULL;
    int idx = (hist_next - hist_count + pos + HIST_SIZE * 2) % HIST_SIZE;
    return hist_buf[idx];
}

/* ── terminal helpers ────────────────────────────────────────────────────── */

static void erase_chars(int n) {
    for (int i = 0; i < n; i++)
        write(STDOUT_FILENO, "\b", 1);
}

/* ── variables ───────────────────────────────────────────────────────────── */

#define VAR_MAX   16
#define VAR_NAME  16
#define VAR_VAL   64

static char var_names[VAR_MAX][VAR_NAME];
static char var_vals[VAR_MAX][VAR_VAL];
static int  var_count = 0;

static const char *var_get(const char *name) {
    for (int i = 0; i < var_count; i++)
        if (strcmp(var_names[i], name) == 0) return var_vals[i];
    return NULL;
}

static void var_set(const char *name, const char *val) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_names[i], name) == 0) {
            strncpy(var_vals[i], val, VAR_VAL - 1);
            var_vals[i][VAR_VAL - 1] = '\0';
            return;
        }
    }
    if (var_count < VAR_MAX) {
        strncpy(var_names[var_count], name, VAR_NAME - 1);
        var_names[var_count][VAR_NAME - 1] = '\0';
        strncpy(var_vals[var_count], val, VAR_VAL - 1);
        var_vals[var_count][VAR_VAL - 1] = '\0';
        var_count++;
    }
}

static void var_unset(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_names[i], name) == 0) {
            /* replace slot with last entry */
            var_count--;
            if (i < var_count) {
                strcpy(var_names[i], var_names[var_count]);
                strcpy(var_vals[i],  var_vals[var_count]);
            }
            return;
        }
    }
}

/* expand_vars: copy src to dst, replacing $name with the stored value. */
static void expand_vars(const char *src, char *dst, int dmax) {
    int di = 0;
    while (*src && di < dmax - 1) {
        if (*src == '$') {
            src++;
            char name[VAR_NAME];
            int ni = 0;
            while (*src && ni < VAR_NAME - 1 &&
                   ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') ||
                    (*src >= '0' && *src <= '9') || *src == '_')) {
                name[ni++] = *src++;
            }
            name[ni] = '\0';
            if (ni > 0) {
                const char *val = var_get(name);
                if (val) {
                    while (*val && di < dmax - 1) dst[di++] = *val++;
                }
            } else {
                if (di < dmax - 1) dst[di++] = '$';
            }
        } else {
            dst[di++] = *src++;
        }
    }
    dst[di] = '\0';
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

    /* Left: stdout -> pipe write. */
    dup2(p[1], STDOUT_FILENO);
    int left_pid = exec(left);
    dup2(saved_out, STDOUT_FILENO);
    close(saved_out);
    close(p[1]);

    /* Right: stdin <- pipe read. */
    dup2(p[0], STDIN_FILENO);
    int right_pid = exec(right);
    dup2(saved_in, STDIN_FILENO);
    close(saved_in);
    close(p[0]);

    if (left_pid  > 0) waitpid(left_pid);
    if (right_pid > 0) waitpid(right_pid);
}

/* ── scripting: blocks ───────────────────────────────────────────────────── */

#define BLOCK_MAX 64

typedef struct {
    char *lines[BLOCK_MAX];
    int   count;
} block_t;

static void block_free(block_t *b) {
    for (int i = 0; i < b->count; i++) {
        free(b->lines[i]);
        b->lines[i] = NULL;
    }
    b->count = 0;
}

/*
 * block_collect: read lines from stdin (showing "..> " prompt) until a bare
 * "end" closes the block.  Nested if/for/loop headers increase depth so their
 * own "end" is consumed correctly.  Returns number of lines stored.
 */
static int block_collect(block_t *b) {
    b->count = 0;
    int depth = 0;

    for (;;) {
        write(STDOUT_FILENO, "..> ", 4);
        char tmp[LINE_MAX];
        readline(tmp, sizeof(tmp));

        /* Determine if this line opens or closes a nesting level. */
        char first[LINE_MAX];
        strncpy(first, tmp, LINE_MAX - 1);
        first[LINE_MAX - 1] = '\0';
        char *sp = first;
        while (*sp && *sp != ' ') sp++;
        *sp = '\0';

        if (!strcmp(first, "if") || !strcmp(first, "for") || !strcmp(first, "loop"))
            depth++;
        else if (!strcmp(first, "end")) {
            if (depth == 0) return b->count;  /* our closing end */
            depth--;
        }

        if (b->count < BLOCK_MAX) {
            char *copy = malloc(strlen(tmp) + 1);
            if (copy) {
                strcpy(copy, tmp);
                b->lines[b->count++] = copy;
            }
        }
    }
}

/* Forward declaration — eval_line is defined after the built-ins. */
static int eval_line(char *line);

static int eval_block(block_t *b);   /* forward decl */

/* ── break flag ──────────────────────────────────────────────────────────── */

static int g_break = 0;

/* ── built-in commands ───────────────────────────────────────────────────── */

/* Forward declaration of cmd_help (defined after command table). */
static int cmd_help(const char *arg);

#define IS_HELP(a) ((a) && strcmp((a), "--help") == 0)

static int cmd_echo(const char *arg) {
    if (IS_HELP(arg)) {
        puts("usage: echo <text>");
        puts("  write text to stdout followed by a newline");
        return 0;
    }
    if (arg) puts(arg);
    else     write(STDOUT_FILENO, "\n", 1);
    return 0;
}

static int cmd_ls(const char *path) {
    if (IS_HELP(path)) {
        puts("usage: ls [path]");
        puts("  list directory entries (default: /)");
        return 0;
    }
    if (!path || !*path) path = "/";
    int fd = open(path, O_RDONLY);
    if (fd < 0) { puts("ls: open failed"); return -1; }

    char name[DIRENT_MAX];
    uint32_t idx = 0;
    while (readdir(fd, idx, name, sizeof(name)) == 0) {
        puts(name);
        idx++;
    }
    close(fd);
    return 0;
}

static int cmd_cat(const char *path) {
    if (IS_HELP(path)) {
        puts("usage: cat <path>");
        puts("  print file to stdout");
        return 0;
    }
    if (!path || !*path) { puts("cat: missing path"); return -1; }
    int fd = open(path, O_RDONLY);
    if (fd < 0) { puts("cat: open failed"); return -1; }

    char buf[128];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, (size_t)n);
    close(fd);
    return 0;
}

static int cmd_exec(const char *path) {
    if (IS_HELP(path)) {
        puts("usage: exec <path>");
        puts("  spawn a program and return immediately (async)");
        return 0;
    }
    if (!path || !*path) { puts("exec: missing path"); return -1; }
    int pid = exec(path);
    if (pid < 0) { puts("exec: failed"); return -1; }
    printf("  pid %d\n", pid);
    return 0;
}

static int cmd_run(const char *path) {
    if (IS_HELP(path)) {
        puts("usage: run <path>");
        puts("  spawn a program and wait for it to exit");
        return 0;
    }
    if (!path || !*path) { puts("run: missing path"); return -1; }
    int pid = exec(path);
    if (pid < 0) { puts("run: exec failed"); return -1; }
    return waitpid(pid);   /* returns child's exit code */
}

static int cmd_rm(const char *path) {
    if (IS_HELP(path)) {
        puts("usage: rm <path>");
        puts("  remove a file; rm /proc/<pid> kills a process");
        return 0;
    }
    if (!path || !*path) { puts("rm: missing path"); return -1; }
    if (unlink(path) < 0) { puts("rm: failed"); return -1; }
    return 0;
}

static int cmd_pid(const char *arg) {
    if (IS_HELP(arg)) {
        puts("usage: pid");
        puts("  print the process ID of this shell");
        return 0;
    }
    printf("  pid: %d\n", getpid());
    return 0;
}

/* ── variable built-ins ──────────────────────────────────────────────────── */

static int cmd_set(const char *arg) {
    if (IS_HELP(arg)) {
        puts("usage: set name value");
        puts("  store value in $name; use in commands as $name");
        return 0;
    }
    if (!arg || !*arg) { puts("set: usage: set name value"); return -1; }

    char name[VAR_NAME];
    int ni = 0;
    const char *p = arg;
    while (*p && *p != ' ' && ni < VAR_NAME - 1) name[ni++] = *p++;
    name[ni] = '\0';
    while (*p == ' ') p++;

    var_set(name, p);
    return 0;
}

static int cmd_unset(const char *arg) {
    if (IS_HELP(arg)) {
        puts("usage: unset name");
        puts("  remove variable $name");
        return 0;
    }
    if (!arg || !*arg) { puts("unset: missing name"); return -1; }
    var_unset(arg);
    return 0;
}

static int cmd_env(const char *arg) {
    (void)arg;
    for (int i = 0; i < var_count; i++) {
        write(STDOUT_FILENO, var_names[i], strlen(var_names[i]));
        write(STDOUT_FILENO, "=", 1);
        puts(var_vals[i]);
    }
    return 0;
}

static int cmd_true(const char *arg)  { (void)arg; return 0;  }
static int cmd_false(const char *arg) { (void)arg; return 1;  }

/* ── control flow built-ins ──────────────────────────────────────────────── */

/*
 * if <cmd>         runs <cmd>; if exit code == 0, executes the block.
 *   ...body...     the body is collected interactively (prompt: "..> ")
 * end
 *
 * The condition is evaluated immediately on the "if" line, before the body
 * is typed.  This is the immediate-evaluation model — simpler than deferred
 * parsing, and natural for both interactive and script use.
 */
static int cmd_if(const char *arg) {
    if (IS_HELP(arg)) {
        puts("usage: if <cmd>");
        puts("  execute block if <cmd> exits 0; terminated by 'end'");
        return 0;
    }

    char cond[LINE_MAX];
    strncpy(cond, arg ? arg : "", LINE_MAX - 1);
    cond[LINE_MAX - 1] = '\0';

    /* Run the condition now, then collect the body. */
    int cond_result = eval_line(cond);

    block_t body;
    block_collect(&body);

    if (cond_result == 0) eval_block(&body);

    block_free(&body);
    return cond_result;
}

/*
 * for <name> in <word> [<word> ...]
 *   ...body...
 * end
 */
static int cmd_for(const char *arg) {
    if (IS_HELP(arg)) {
        puts("usage: for name in word [word ...]");
        puts("  iterate over words, setting $name each time");
        return 0;
    }
    if (!arg || !*arg) { puts("for: usage: for name in words"); return -1; }

    char buf[LINE_MAX];
    strncpy(buf, arg, LINE_MAX - 1);
    buf[LINE_MAX - 1] = '\0';

    /* Parse: name */
    char *p = buf;
    char *name = p;
    while (*p && *p != ' ') p++;
    if (*p) *p++ = '\0';
    while (*p == ' ') p++;

    /* Expect "in" */
    if (strncmp(p, "in", 2) != 0 || (p[2] != ' ' && p[2] != '\0')) {
        puts("for: expected 'in' after name");
        return -1;
    }
    p += 2;
    while (*p == ' ') p++;

    /* Collect the body block. */
    block_t body;
    block_collect(&body);

    /* Iterate over words. */
    g_break = 0;
    while (*p && !g_break) {
        char word[VAR_VAL];
        int wi = 0;
        while (*p && *p != ' ' && wi < VAR_VAL - 1) word[wi++] = *p++;
        word[wi] = '\0';
        while (*p == ' ') p++;
        if (!word[0]) break;
        var_set(name, word);
        eval_block(&body);
    }
    g_break = 0;

    block_free(&body);
    return 0;
}

/*
 * loop
 *   ...body...
 * end
 *
 * Runs the body forever until 'break' is executed inside it.
 */
static int cmd_loop(const char *arg) {
    if (IS_HELP(arg)) {
        puts("usage: loop");
        puts("  repeat block until 'break'; terminated by 'end'");
        return 0;
    }

    block_t body;
    block_collect(&body);

    g_break = 0;
    while (!g_break) {
        eval_block(&body);
    }
    g_break = 0;

    block_free(&body);
    return 0;
}

static int cmd_break(const char *arg) {
    if (IS_HELP(arg)) { puts("usage: break"); puts("  exit the innermost loop"); return 0; }
    g_break = 1;
    return 0;
}

/* ── command table ───────────────────────────────────────────────────────── */

typedef int (*cmd_fn_t)(const char *);

typedef struct {
    const char *name;
    cmd_fn_t    fn;
    const char *usage;
    const char *desc;
} cmd_entry_t;

static const cmd_entry_t cmds[] = {
    /* shell */
    { "help",  cmd_help,  "help [cmd]",              "list commands, or describe one"  },
    { "echo",  cmd_echo,  "echo <text>",             "write text to stdout"            },
    { "ls",    cmd_ls,    "ls [path]",               "list directory"                  },
    { "cat",   cmd_cat,   "cat <path>",              "print file to stdout"            },
    { "exec",  cmd_exec,  "exec <path>",             "spawn program (no wait)"         },
    { "run",   cmd_run,   "run <path>",              "spawn and wait for exit"         },
    { "rm",    cmd_rm,    "rm <path>",               "remove file (or kill /proc/pid)" },
    { "pid",   cmd_pid,   "pid",                     "print our process ID"            },
    /* variables */
    { "set",   cmd_set,   "set name value",          "store shell variable"            },
    { "unset", cmd_unset, "unset name",              "remove shell variable"           },
    { "env",   cmd_env,   "env",                     "list all shell variables"        },
    { "true",  cmd_true,  "true",                    "exit 0 (success)"                },
    { "false", cmd_false, "false",                   "exit 1 (failure)"                },
    /* control flow */
    { "if",    cmd_if,    "if <cmd>",                "run block if cmd exits 0"        },
    { "for",   cmd_for,   "for name in words",       "iterate over words"              },
    { "loop",  cmd_loop,  "loop",                    "repeat block until break"        },
    { "break", cmd_break, "break",                   "exit innermost loop"             },
    { NULL,    NULL,      NULL,                       NULL                             },
};

/* ── help ────────────────────────────────────────────────────────────────── */

static int cmd_help(const char *arg) {
    if (arg && *arg) {
        if (strcmp(arg, "--help") == 0) {
            puts("usage: help [cmd]");
            puts("  list all built-in commands, or describe one");
            return 0;
        }
        for (int i = 0; cmds[i].name; i++) {
            if (strcmp(cmds[i].name, arg) == 0) {
                puts(cmds[i].usage);
                puts(cmds[i].desc);
                return 0;
            }
        }
        puts("sh: help: unknown command");
        return -1;
    }

    puts("Commands:");
    for (int i = 0; cmds[i].name; i++) {
        write(STDOUT_FILENO, "  ", 2);
        int ulen = (int)strlen(cmds[i].usage);
        write(STDOUT_FILENO, cmds[i].usage, (size_t)ulen);
        for (int pad = ulen; pad < 22; pad++) write(STDOUT_FILENO, " ", 1);
        puts(cmds[i].desc);
    }
    puts("Keys:");
    puts("  Up/Down               history navigation");
    puts("  Tab                   path completion");
    puts("Syntax:");
    puts("  left | right          pipeline (external programs)");
    puts("  $name                 variable expansion");
    return 0;
}

/* ── line evaluator ──────────────────────────────────────────────────────── */

/*
 * eval_block: run each line in a block, stopping early on break.
 */
static int eval_block(block_t *b) {
    int result = 0;
    for (int i = 0; i < b->count && !g_break; i++) {
        char tmp[LINE_MAX];
        strncpy(tmp, b->lines[i], LINE_MAX - 1);
        tmp[LINE_MAX - 1] = '\0';
        result = eval_line(tmp);
    }
    return result;
}

/*
 * eval_line: evaluate one command line.
 *  - expands $variables
 *  - handles pipelines
 *  - dispatches to built-ins or external paths
 * Returns the command's exit code (0 = success, non-zero = failure).
 */
static int eval_line(char *line) {
    if (!line) return 0;

    /* trim leading spaces */
    while (*line == ' ') line++;
    if (!*line || *line == '#') return 0;   /* empty or comment */

    /* variable expansion */
    char expanded[LINE_MAX * 2];
    expand_vars(line, expanded, (int)sizeof(expanded));

    /* pipeline check */
    char *pipe_pos = NULL;
    for (int i = 0; expanded[i]; i++) {
        if (expanded[i] == '|') { pipe_pos = &expanded[i]; break; }
    }
    if (pipe_pos) {
        *pipe_pos = '\0';
        execute_pipeline(expanded, pipe_pos + 1);
        return 0;   /* pipeline exit code not propagated in v1 */
    }

    /* split command + arg */
    char *arg = split_arg(expanded);
    const char *cmd = expanded;

    /* look up in built-in table */
    for (int i = 0; cmds[i].name; i++) {
        if (strcmp(cmd, cmds[i].name) == 0)
            return cmds[i].fn(arg);
    }

    /* external path: anything starting with / */
    if (cmd[0] == '/') {
        const char *argv_arr[3];
        int narg = 1;
        argv_arr[0] = cmd;
        if (arg && arg[0]) { argv_arr[1] = arg; narg = 2; }
        argv_arr[narg] = NULL;
        int pid = execv(cmd, argv_arr, narg);
        if (pid < 0) { puts("sh: exec failed"); return -1; }
        return waitpid(pid);
    }

    puts("sh: unknown command");
    return -1;
}

/* ── main loop ───────────────────────────────────────────────────────────── */

int main(void) {
    puts("DOS/9 sh - type 'help' for commands");

    char line[LINE_MAX];
    for (;;) {
        write(STDOUT_FILENO, "sh> ", 4);
        if (readline(line, sizeof(line)) == 0) continue;
        hist_add(line);
        eval_line(line);
    }
}

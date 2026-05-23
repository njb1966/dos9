#include <dos9.h>

#define LINE_MAX    128
#define PATH_MAX     64
#define DIRENT_MAX   64
#define HIST_SIZE    32
#define ARG_MAX     16

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

static int var_set(const char *name, const char *val) {
    if (!name || !*name || !val) return -1;
    if (strlen(name) >= VAR_NAME) return -1;
    if (strlen(val) >= VAR_VAL) return -1;
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_names[i], name) == 0) {
            memcpy(var_vals[i], val, strlen(val) + 1);
            return 0;
        }
    }
    if (var_count < VAR_MAX) {
        memcpy(var_names[var_count], name, strlen(name) + 1);
        memcpy(var_vals[var_count], val, strlen(val) + 1);
        var_count++;
        return 0;
    }
    return -1;
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

/* expand_vars: copy src to dst, replacing $name with the stored value.
   Returns 0 on success, -1 on malformed input (e.g. an overlong name). */
static int expand_vars(const char *src, char *dst, int dmax) {
    int di = 0;
    while (*src) {
        if (*src == '$') {
            src++;
            char name[VAR_NAME];
            int ni = 0;
            while (*src &&
                   ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') ||
                    (*src >= '0' && *src <= '9') || *src == '_')) {
                if (ni >= VAR_NAME - 1) return -1;
                name[ni++] = *src++;
            }
            name[ni] = '\0';
            if (ni > 0) {
                const char *val = var_get(name);
                if (val) {
                    while (*val) {
                        if (di >= dmax - 1) return -1;
                        dst[di++] = *val++;
                    }
                }
            } else {
                if (di >= dmax - 1) return -1;
                dst[di++] = '$';
            }
        } else {
            if (di >= dmax - 1) return -1;
            dst[di++] = *src++;
        }
    }
    dst[di] = '\0';
    return 0;
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
            if ((int)strlen(entry) >= DIRENT_MAX) continue;
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
    int  saw_eof  = 0;
    char saved[LINE_MAX];
    saved[0] = '\0';
    buf[0]   = '\0';

    for (;;) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            saw_eof = 1;
            break;
        }

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
            memcpy(buf, h, (size_t)len);
            buf[len] = '\0';
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
    if (saw_eof && len == 0)
        return -1;
    return len;
}

/* ── argument parsing ────────────────────────────────────────────────────── */

static const char *skip_spaces(const char *p) {
    while (*p == ' ') p++;
    return p;
}

/*
 * Parse one shell token from `*pp` into `dst`, handling:
 *   - spaces as separators
 *   - single/double quotes
 *   - backslash escapes outside quotes and inside double quotes
 * Returns 1 if a token was parsed, 0 at end of input, -1 on malformed input.
 */
static int parse_word(const char **pp, char *dst, int max) {
    const char *p = skip_spaces(*pp);
    int di = 0;
    if (!*p) {
        *pp = p;
        return 0;
    }

    if (*p == '|') {
        if (max <= 1) return -1;
        dst[0] = '|';
        dst[1] = '\0';
        *pp = p + 1;
        return 2;
    }

    while (*p && *p != ' ' && *p != '|') {
        char c = *p++;
        if (c == '\\' && *p) {
            if (*p == ' ' || *p == '|') {
                if (di >= max - 1) return -1;
                dst[di++] = *p++;
                continue;
            }
            c = *p++;
            if (di >= max - 1) return -1;
            dst[di++] = c;
            continue;
        }
        if (c == '\\' && !*p) return -1;
        if (c == '\'' || c == '"') {
            char quote = c;
            while (*p && *p != quote) {
                c = *p++;
                if (quote == '"' && c == '\\' && *p) c = *p++;
                if (di >= max - 1) return -1;
                dst[di++] = c;
            }
            if (*p != quote) return -1;
            p++;
            continue;
        }
        if (di >= max - 1) return -1;
        dst[di++] = c;
    }

    dst[di] = '\0';
    *pp = skip_spaces(p);
    return 1;
}

static int split_argv(const char *line, char argv_buf[ARG_MAX][LINE_MAX],
                      char **argv, int *pipe_at) {
    int argc = 0;
    const char *p = line;
    if (pipe_at) *pipe_at = -1;
    while (argc < ARG_MAX) {
        int r = parse_word(&p, argv_buf[argc], LINE_MAX);
        if (r < 0) return -1;
        if (r == 0) break;
        if (r == 2) {
            if (pipe_at && *pipe_at >= 0) return -1;
            if (pipe_at) *pipe_at = argc;
            continue;
        }
        argv[argc] = argv_buf[argc];
        argc++;
    }
    if (argc == ARG_MAX) {
        const char *q = skip_spaces(p);
        if (*q) return -1;
    }
    argv[argc] = NULL;
    return argc;
}

/* ── pipeline ────────────────────────────────────────────────────────────── */

static int execute_pipeline(int left_argc, char **left_argv,
                            int right_argc, char **right_argv) {
    if (left_argc <= 0 || right_argc <= 0 || !left_argv[0] || !right_argv[0]) {
        puts("pipe: empty command");
        return -1;
    }
    if (left_argv[0][0] != '/' || right_argv[0][0] != '/') {
        puts("pipe: external programs only");
        return -1;
    }

    const char *left_vec[ARG_MAX + 1];
    const char *right_vec[ARG_MAX + 1];
    for (int i = 0; i < left_argc; i++) left_vec[i] = left_argv[i];
    left_vec[left_argc] = NULL;
    for (int i = 0; i < right_argc; i++) right_vec[i] = right_argv[i];
    right_vec[right_argc] = NULL;

    int p[2];
    if (pipe(p) < 0) { puts("pipe: failed"); return -1; }

    int saved_out = dup(STDOUT_FILENO);
    int saved_in  = dup(STDIN_FILENO);
    if (saved_out < 0 || saved_in < 0) {
        puts("pipe: dup failed");
        close(p[0]); close(p[1]);
        if (saved_out >= 0) close(saved_out);
        if (saved_in  >= 0) close(saved_in);
        return -1;
    }

    /* Left: stdout -> pipe write. */
    dup2(p[1], STDOUT_FILENO);
    int left_pid = execv(left_vec[0], left_vec, left_argc);
    dup2(saved_out, STDOUT_FILENO);
    close(saved_out);
    close(p[1]);

    /* Right: stdin <- pipe read. */
    dup2(p[0], STDIN_FILENO);
    int right_pid = execv(right_vec[0], right_vec, right_argc);
    dup2(saved_in, STDIN_FILENO);
    close(saved_in);
    close(p[0]);

    if (left_pid  > 0) waitpid(left_pid);
    if (right_pid > 0) waitpid(right_pid);
    return 0;
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
    int overflow = 0;

    for (;;) {
        write(STDOUT_FILENO, "..> ", 4);
        char tmp[LINE_MAX];
        if (readline(tmp, sizeof(tmp)) < 0)
            return -1;

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
            if (depth == 0) return overflow ? -1 : b->count;  /* our closing end */
            depth--;
        }

        if (!overflow) {
            if (b->count >= BLOCK_MAX) {
                puts("block: too many lines");
                overflow = 1;
                continue;
            }
            char *copy = malloc(strlen(tmp) + 1);
            if (copy) {
                strcpy(copy, tmp);
                b->lines[b->count++] = copy;
            } else {
                puts("block: out of memory");
                overflow = 1;
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
static int eval_argv(int argc, char **argv);
static int cmd_help(int argc, char **argv);

#define IS_HELP(argc, argv) ((argc) > 0 && (argv)[0] && strcmp((argv)[0], "--help") == 0)

static int cmd_echo(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: echo <text>");
        puts("  write text to stdout followed by a newline");
        return 0;
    }
    if (argc <= 0) {
        write(STDOUT_FILENO, "\n", 1);
        return 0;
    }
    for (int i = 0; i < argc; i++) {
        if (i > 0) write(STDOUT_FILENO, " ", 1);
        write(STDOUT_FILENO, argv[i], strlen(argv[i]));
    }
    write(STDOUT_FILENO, "\n", 1);
    return 0;
}

static int cmd_ls(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: ls [path]");
        puts("  list directory entries (default: /)");
        return 0;
    }
    const char *path = (argc > 0 && argv[0] && *argv[0]) ? argv[0] : "/";
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

static int cmd_cat(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: cat <path>");
        puts("  print file to stdout");
        return 0;
    }
    if (argc <= 0 || !argv[0] || !*argv[0]) { puts("cat: missing path"); return -1; }
    const char *path = argv[0];
    int fd = open(path, O_RDONLY);
    if (fd < 0) { puts("cat: open failed"); return -1; }

    char buf[128];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, (size_t)n);
    close(fd);
    return 0;
}

static int cmd_exec(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: exec <path>");
        puts("  spawn a program and return immediately (async)");
        return 0;
    }
    if (argc <= 0 || !argv[0] || !*argv[0]) { puts("exec: missing path"); return -1; }
    int pid = execv(argv[0], (const char **)argv, argc);
    if (pid < 0) { puts("exec: failed"); return -1; }
    printf("  pid %d\n", pid);
    return 0;
}

static int cmd_run(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: run <path>");
        puts("  spawn a program and wait for it to exit");
        return 0;
    }
    if (argc <= 0 || !argv[0] || !*argv[0]) { puts("run: missing path"); return -1; }
    int pid = execv(argv[0], (const char **)argv, argc);
    if (pid < 0) { puts("run: exec failed"); return -1; }
    return waitpid(pid);   /* returns child's exit code */
}

static int cmd_rm(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: rm <path>");
        puts("  remove a file; rm /proc/<pid> kills a process");
        return 0;
    }
    if (argc <= 0 || !argv[0] || !*argv[0]) { puts("rm: missing path"); return -1; }
    if (unlink(argv[0]) < 0) { puts("rm: failed"); return -1; }
    return 0;
}

static int cmd_pid(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: pid");
        puts("  print the process ID of this shell");
        return 0;
    }
    printf("  pid: %d\n", getpid());
    return 0;
}

/* ── variable built-ins ──────────────────────────────────────────────────── */

static int cmd_set(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: set name value");
        puts("  store value in $name; use in commands as $name");
        return 0;
    }
    if (argc < 2 || !argv[0] || !*argv[0]) { puts("set: usage: set name value"); return -1; }
    char value[VAR_VAL];
    value[0] = '\0';
    for (int i = 1; i < argc; i++) {
        size_t used = strlen(value);
        if (i > 1) {
            if (used + 1 >= VAR_VAL) {
                puts("set: value too long");
                return -1;
            }
            value[used++] = ' ';
            value[used] = '\0';
        }
        const char *src = argv[i];
        while (*src) {
            if (used >= VAR_VAL - 1) {
                puts("set: value too long");
                return -1;
            }
            value[used++] = *src++;
        }
        value[used] = '\0';
    }

    if (var_set(argv[0], value) < 0) {
        puts("set: invalid variable");
        return -1;
    }
    return 0;
}

static int cmd_unset(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: unset name");
        puts("  remove variable $name");
        return 0;
    }
    if (argc <= 0 || !argv[0] || !*argv[0]) { puts("unset: missing name"); return -1; }
    var_unset(argv[0]);
    return 0;
}

static int cmd_env(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < var_count; i++) {
        write(STDOUT_FILENO, var_names[i], strlen(var_names[i]));
        write(STDOUT_FILENO, "=", 1);
        puts(var_vals[i]);
    }
    return 0;
}

static int cmd_true(int argc, char **argv)  { (void)argc; (void)argv; return 0;  }
static int cmd_false(int argc, char **argv) { (void)argc; (void)argv; return 1;  }

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
static int cmd_if(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: if <cmd>");
        puts("  execute block if <cmd> exits 0; terminated by 'end'");
        return 0;
    }

    /* Run the condition now, then collect the body. */
    int cond_result = (argc > 0) ? eval_argv(argc, argv) : -1;

    block_t body;
    if (block_collect(&body) < 0) {
        block_free(&body);
        return -1;
    }

    if (cond_result == 0) eval_block(&body);

    block_free(&body);
    return cond_result;
}

/*
 * for <name> in <word> [<word> ...]
 *   ...body...
 * end
 */
static int cmd_for(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: for name in word [word ...]");
        puts("  iterate over words, setting $name each time");
        return 0;
    }
    if (argc < 3) { puts("for: usage: for name in words"); return -1; }

    if (strcmp(argv[1], "in") != 0) {
        puts("for: expected 'in' after name");
        return -1;
    }

    /* Collect the body block. */
    block_t body;
    if (block_collect(&body) < 0) {
        block_free(&body);
        return -1;
    }

    /* Iterate over words. */
    g_break = 0;
    for (int i = 2; i < argc && !g_break; i++) {
        if (var_set(argv[0], argv[i]) < 0) {
            puts("for: variable too long");
            g_break = 0;
            block_free(&body);
            return -1;
        }
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
static int cmd_loop(int argc, char **argv) {
    if (IS_HELP(argc, argv)) {
        puts("usage: loop");
        puts("  repeat block until 'break'; terminated by 'end'");
        return 0;
    }

    block_t body;
    if (block_collect(&body) < 0) {
        block_free(&body);
        return -1;
    }

    g_break = 0;
    while (!g_break) {
        eval_block(&body);
    }
    g_break = 0;

    block_free(&body);
    return 0;
}

static int cmd_break(int argc, char **argv) {
    if (IS_HELP(argc, argv)) { puts("usage: break"); puts("  exit the innermost loop"); return 0; }
    g_break = 1;
    return 0;
}

/* ── command table ───────────────────────────────────────────────────────── */

typedef int (*cmd_fn_t)(int argc, char **argv);

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

static int cmd_help(int argc, char **argv) {
    if (argc > 0 && argv[0] && *argv[0]) {
        if (strcmp(argv[0], "--help") == 0) {
            puts("usage: help [cmd]");
            puts("  list all built-in commands, or describe one");
            return 0;
        }
        for (int i = 0; cmds[i].name; i++) {
            if (strcmp(cmds[i].name, argv[0]) == 0) {
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
 * eval_argv: evaluate a parsed argv vector.
 *  - handles pipelines
 *  - dispatches to built-ins or external paths
 * Returns the command's exit code (0 = success, non-zero = failure).
 */
static int eval_argv(int argc, char **argv) {
    if (argc <= 0 || !argv || !argv[0] || !argv[0][0]) return 0;

    for (int i = 0; cmds[i].name; i++) {
        if (strcmp(argv[0], cmds[i].name) == 0)
            return cmds[i].fn(argc - 1, argv + 1);
    }

    if (argv[0][0] == '/') {
        int pid = execv(argv[0], (const char **)argv, argc);
        if (pid < 0) { puts("sh: exec failed"); return -1; }
        return waitpid(pid);
    }

    puts("sh: unknown command");
    return -1;
}

/*
 * eval_line: evaluate one command line.
 *  - expands $variables
 *  - tokenizes with quoting/escaping support
 *  - dispatches through eval_argv()
 * Returns the command's exit code (0 = success, non-zero = failure).
 */
static int eval_line(char *line) {
    if (!line) return 0;

    while (*line == ' ') line++;
    if (!*line || *line == '#') return 0;

    char expanded[LINE_MAX * 2];
    if (expand_vars(line, expanded, (int)sizeof(expanded)) < 0) {
        puts("sh: parse error");
        return -1;
    }

    char argv_buf[ARG_MAX][LINE_MAX];
    char *argv[ARG_MAX + 1];
    int pipe_at = -1;
    int argc = split_argv(expanded, argv_buf, argv, &pipe_at);
    if (argc < 0) {
        puts("sh: parse error");
        return -1;
    }
    if (pipe_at >= 0) {
        if (pipe_at <= 0 || pipe_at >= argc) {
            puts("pipe: empty command");
            return -1;
        }
        return execute_pipeline(pipe_at, argv, argc - pipe_at, argv + pipe_at);
    }
    return eval_argv(argc, argv);
}

/* ── main loop ───────────────────────────────────────────────────────────── */

int main(void) {
    puts("DOS/9 sh - type 'help' for commands");

    char line[LINE_MAX];
    for (;;) {
        write(STDOUT_FILENO, "sh> ", 4);
        int n = readline(line, sizeof(line));
        if (n < 0) break;
        if (n == 0) continue;
        hist_add(line);
        eval_line(line);
    }
}

#include <dos9.h>

#define LINE_MAX 128
#define PATH_MAX 64
#define DIRENT_MAX 64

/* ── line input ─────────────────────────────────────────────────────────── */

static int readline(char *buf, int max) {
    int i = 0;
    char c;
    while (i < max - 1) {
        if (read(STDIN_FILENO, &c, 1) != 1) break;
        if (c == '\r' || c == '\n') { write(STDOUT_FILENO, "\n", 1); break; }
        if ((c == '\b' || c == 127) && i > 0) {
            i--;
            write(STDOUT_FILENO, "\b \b", 3);
            continue;
        }
        if (c < 32) continue;
        buf[i++] = c;
        write(STDOUT_FILENO, &c, 1);
    }
    buf[i] = '\0';
    return i;
}

/* ── argument splitting ──────────────────────────────────────────────────── */

/* Split buf in-place at first space; returns pointer to arg or NULL. */
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
    int r;
    while ((r = readdir(fd, idx, name, sizeof(name))) == 0) {
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
    puts("DOS/9 sh — type 'help' for commands");

    char line[LINE_MAX];
    for (;;) {
        write(STDOUT_FILENO, "sh> ", 4);
        if (readline(line, sizeof(line)) == 0) continue;

        char *arg = split_arg(line);
        const char *cmd = line;

        if (strcmp(cmd, "help") == 0)       cmd_help();
        else if (strcmp(cmd, "echo") == 0)  cmd_echo(arg);
        else if (strcmp(cmd, "ls") == 0)    cmd_ls(arg);
        else if (strcmp(cmd, "cat") == 0)   cmd_cat(arg);
        else if (strcmp(cmd, "exec") == 0)  cmd_exec(arg);
        else if (strcmp(cmd, "run") == 0)   cmd_run(arg);
        else if (strcmp(cmd, "rm") == 0)    cmd_rm(arg);
        else if (strcmp(cmd, "pid") == 0)   cmd_pid();
        else { puts("sh: unknown command"); }
    }
}

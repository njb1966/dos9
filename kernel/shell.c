#include <shell.h>
#include <terminal.h>
#include <keyboard.h>
#include <vfs.h>
#include <io.h>
#include <string.h>
#include <syscall.h>
#include <stddef.h>

#define LINE_MAX 256

static char line[LINE_MAX];

/* Return pointer past prefix+spaces in str, or NULL if prefix doesn't match. */
static const char *match(const char *str, const char *prefix) {
    while (*prefix && *str == *prefix) { str++; prefix++; }
    if (*prefix) return NULL;
    while (*str == ' ') str++;
    return str;
}

/* ── built-in commands ───────────────────────────────────────────────── */

static void cmd_help(void) {
    terminal_write("Commands:\n");
    terminal_write("  help            this message\n");
    terminal_write("  halt            halt the CPU\n");
    terminal_write("  echo [text]     write text via int 0x80 syscall\n");
    terminal_write("  ls [path]       list directory (default: /)\n");
    terminal_write("  cat [path]      print file contents\n");
    terminal_write("Paths:  /dev  /proc\n");
}

static void cmd_echo(const char *arg) {
    uint32_t len = strlen(arg);
    int32_t ret;
    /* SYS_WRITE(fd=1, buf=arg, len) via int 0x80 */
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "0"((int32_t)SYS_WRITE),
          "b"((int32_t)1),
          "c"(arg),
          "d"(len)
        : "memory"
    );
    terminal_putchar('\n');
}

static void cmd_halt(void) {
    terminal_write("Halting.\n");
    __asm__ volatile("cli; hlt");
    __builtin_unreachable();
}

static void cmd_ls(const char *path) {
    if (!*path) path = "/";
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        terminal_write("ls: not found: ");
        terminal_write(path);
        terminal_putchar('\n');
        return;
    }
    char name[128];
    for (uint32_t i = 0; vfs_readdir(fd, i, name, sizeof(name)) == 0; i++) {
        terminal_write(name);
        terminal_putchar('\n');
    }
    vfs_close(fd);
}

static void cmd_cat(const char *path) {
    if (!*path) {
        terminal_write("cat: missing path\n");
        return;
    }
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        terminal_write("cat: not found: ");
        terminal_write(path);
        terminal_putchar('\n');
        return;
    }
    char buf[128];
    int n;
    while ((n = vfs_read(fd, buf, sizeof(buf))) > 0)
        for (int i = 0; i < n; i++) terminal_putchar(buf[i]);
    vfs_close(fd);
}

/* ── command dispatch ────────────────────────────────────────────────── */

static void execute(const char *cmd) {
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;

    const char *arg;
    if ((arg = match(cmd, "help")) && !*arg) { cmd_help();    return; }
    if ((arg = match(cmd, "halt")) && !*arg) { cmd_halt();    return; }
    if ((arg = match(cmd, "echo")))          { cmd_echo(arg); return; }
    if ((arg = match(cmd, "ls")))            { cmd_ls(arg);   return; }
    if ((arg = match(cmd, "cat")))           { cmd_cat(arg);  return; }

    terminal_write("Unknown command: ");
    terminal_write(cmd);
    terminal_putchar('\n');
}

void shell_run(void) {
    for (;;) {
        terminal_write("DOS/9> ");

        int len = 0;
        for (;;) {
            char c = kbd_getchar();

            if (c == '\n' || c == '\r') {
                terminal_putchar('\n');
                line[len] = '\0';
                break;
            }
            if (c == '\b') {
                if (len > 0) { len--; terminal_putchar('\b'); }
                continue;
            }
            if (len < LINE_MAX - 1) {
                line[len++] = c;
                terminal_putchar(c);
            }
        }

        execute(line);
    }
}

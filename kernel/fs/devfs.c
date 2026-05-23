#include <devfs.h>
#include <vfs.h>
#include <rtc.h>
#include <terminal.h>
#include <keyboard.h>
#include <string.h>
#include <stddef.h>

/* ── /dev/vga ────────────────────────────────────────────────────────── */

static int vga_write(vnode_t *v, const void *buf, uint32_t off, uint32_t len) {
    (void)v; (void)off;
    const char *p = buf;
    for (uint32_t i = 0; i < len; i++) terminal_putchar(p[i]);
    return (int)len;
}

static fs_ops_t vga_ops = { .write = vga_write };

static vnode_t dev_vga = {
    .type = VTYPE_CHR, .size = 0, .priv = NULL, .ops = &vga_ops,
};

/* ── /dev/kbd ────────────────────────────────────────────────────────── */

static int kbd_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    (void)v; (void)off;
    if (!len) return 0;
    char *p = buf;
    /* Block until at least one character is available. */
    while (!kbd_haschar())
        __asm__ volatile("sti; hlt");
    uint32_t n = 0;
    while (n < len && kbd_haschar()) {
        p[n] = kbd_getchar();
        if (p[n++] == '\n') break;
    }
    return (int)n;
}

static fs_ops_t kbd_ops = { .read = kbd_read };

static vnode_t dev_kbd = {
    .type = VTYPE_CHR, .size = 0, .priv = NULL, .ops = &kbd_ops,
};

/* ── /dev/null ───────────────────────────────────────────────────────── */

static int null_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    (void)v; (void)buf; (void)off; (void)len;
    return 0;   /* EOF immediately */
}

static int null_write(vnode_t *v, const void *buf, uint32_t off, uint32_t len) {
    (void)v; (void)buf; (void)off;
    return (int)len;    /* swallow all bytes */
}

static fs_ops_t null_ops = { .read = null_read, .write = null_write };

static vnode_t dev_null = {
    .type = VTYPE_CHR, .size = 0, .priv = NULL, .ops = &null_ops,
};

/* ── /dev/time ───────────────────────────────────────────────────────── */

static int time_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    (void)v;
    if (off > 0 || len == 0) return 0;
    char tmp[32];
    uint32_t now = rtc_unix_seconds();
    char rev[16];
    int n = 0;
    if (now == 0) {
        tmp[0] = '0';
        tmp[1] = '\n';
        tmp[2] = '\0';
        n = 2;
    } else {
        while (now > 0 && n < (int)sizeof(rev)) {
            rev[n++] = (char)('0' + (now % 10u));
            now /= 10u;
        }
        int i = 0;
        while (n > 0) tmp[i++] = rev[--n];
        tmp[i++] = '\n';
        tmp[i] = '\0';
        n = i;
    }
    uint32_t copy = (uint32_t)n < len ? (uint32_t)n : len;
    memcpy(buf, tmp, copy);
    return (int)copy;
}

static fs_ops_t time_ops = { .read = time_read };

static vnode_t dev_time = {
    .type = VTYPE_FILE, .size = 0, .priv = NULL, .ops = &time_ops,
};

/* ── devfs directory ─────────────────────────────────────────────────── */

typedef struct { const char *name; vnode_t *vnode; } dev_entry_t;

static const dev_entry_t devdir[] = {
    { "vga",  &dev_vga  },
    { "kbd",  &dev_kbd  },
    { "null", &dev_null },
    { "time", &dev_time },
};

#define N_DEVS ((uint32_t)(sizeof(devdir) / sizeof(devdir[0])))

static vnode_t *devfs_lookup(vnode_t *dir, const char *name) {
    (void)dir;
    for (uint32_t i = 0; i < N_DEVS; i++)
        if (strcmp(devdir[i].name, name) == 0) return devdir[i].vnode;
    return NULL;
}

static int devfs_readdir(vnode_t *dir, uint32_t idx,
                          char *name_out, uint32_t nmax) {
    (void)dir;
    if (idx >= N_DEVS) return -1;
    if (nmax == 0) return -1;
    uint32_t i = 0;
    while (i + 1u < nmax && devdir[idx].name[i]) {
        name_out[i] = devdir[idx].name[i];
        i++;
    }
    name_out[i] = '\0';
    return 0;
}

static fs_ops_t devfs_dir_ops = {
    .lookup  = devfs_lookup,
    .readdir = devfs_readdir,
};

static vnode_t devfs_root = {
    .type = VTYPE_DIR, .size = 0, .priv = NULL, .ops = &devfs_dir_ops,
};

void devfs_init(void) {
    vfs_mount("/dev", &devfs_root);
}

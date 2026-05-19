#include <modfs.h>
#include <vfs.h>
#include <pmm.h>
#include <kernel.h>
#include <multiboot.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_MOD_FILES 8

static vnode_t mod_files[MAX_MOD_FILES];

/* ── helpers ─────────────────────────────────────────────────────────── */

static uint32_t uint_to_str(uint32_t n, char *buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return 1; }
    char tmp[12];
    uint32_t i = 0;
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    for (uint32_t j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
    return i;
}

/* ── /mod/<N> file ───────────────────────────────────────────────────── */

static int mod_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    uint32_t idx = (uint32_t)(uintptr_t)v->priv;
    struct multiboot_mod *m = pmm_mod(idx);
    if (!m) return -1;
    uint32_t size = m->mod_end - m->mod_start;
    if (off >= size) return 0;
    uint32_t avail = size - off;
    uint32_t n = (avail < len) ? avail : len;
    const uint8_t *src = (const uint8_t *)VIRT(m->mod_start) + off;
    memcpy(buf, src, n);
    return (int)n;
}

static fs_ops_t mod_file_ops = { .read = mod_read };

/* ── /mod directory ──────────────────────────────────────────────────── */

static vnode_t *modfs_lookup(vnode_t *dir, const char *name) {
    (void)dir;
    uint32_t idx = 0;
    if (!*name) return NULL;
    for (const char *s = name; *s; s++) {
        if (*s < '0' || *s > '9') return NULL;
        idx = idx * 10 + (uint32_t)(*s - '0');
    }
    if (idx >= pmm_mod_count() || idx >= MAX_MOD_FILES) return NULL;
    return &mod_files[idx];
}

static int modfs_readdir(vnode_t *dir, uint32_t idx,
                          char *name_out, uint32_t nmax) {
    (void)dir;
    uint32_t n = pmm_mod_count();
    if (n > MAX_MOD_FILES) n = MAX_MOD_FILES;
    if (idx >= n) return -1;
    char buf[12];
    uint_to_str(idx, buf);
    strncpy(name_out, buf, nmax);
    return 0;
}

static fs_ops_t modfs_dir_ops = {
    .lookup  = modfs_lookup,
    .readdir = modfs_readdir,
};

static vnode_t modfs_root = {
    .type = VTYPE_DIR, .size = 0, .priv = NULL, .ops = &modfs_dir_ops,
};

void modfs_init(void) {
    for (int i = 0; i < MAX_MOD_FILES; i++) {
        mod_files[i].type = VTYPE_FILE;
        mod_files[i].priv = (void *)(uintptr_t)i;
        mod_files[i].ops  = &mod_file_ops;
        struct multiboot_mod *m = pmm_mod((uint32_t)i);
        mod_files[i].size = m ? (m->mod_end - m->mod_start) : 0;
    }
    vfs_mount("/mod", &modfs_root);
}

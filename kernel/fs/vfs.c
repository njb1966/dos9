#include <vfs.h>
#include <string.h>
#include <stddef.h>

/* Synthetic filesystem roots — defined in their respective .c files */
extern vnode_t devfs_root;
extern vnode_t procfs_root;

/* ── rootfs ──────────────────────────────────────────────────────────── */

static vnode_t *rootfs_lookup(vnode_t *dir, const char *name) {
    (void)dir;
    if (strcmp(name, "dev")  == 0) return &devfs_root;
    if (strcmp(name, "proc") == 0) return &procfs_root;
    return NULL;
}

static int rootfs_readdir(vnode_t *dir, uint32_t idx,
                           char *name_out, uint32_t nmax) {
    (void)dir;
    static const char *entries[] = { "dev", "proc" };
    if (idx >= 2) return -1;
    strncpy(name_out, entries[idx], nmax);
    return 0;
}

static fs_ops_t rootfs_ops = {
    .lookup  = rootfs_lookup,
    .readdir = rootfs_readdir,
};

static vnode_t vfs_root_vnode = {
    .type = VTYPE_DIR,
    .size = 0,
    .priv = NULL,
    .ops  = &rootfs_ops,
};

/* ── path lookup ─────────────────────────────────────────────────────── */

vnode_t *vfs_lookup(const char *path) {
    if (!path || path[0] != '/') return NULL;

    vnode_t *cur = &vfs_root_vnode;
    path++;                     /* skip leading '/' */

    while (*path) {
        /* extract next component */
        char name[128];
        uint32_t i = 0;
        while (*path && *path != '/' && i < sizeof(name) - 1)
            name[i++] = *path++;
        name[i] = '\0';
        if (*path == '/') path++;
        if (!name[0]) continue; /* trailing slash */

        if (!cur->ops || !cur->ops->lookup) return NULL;
        cur = cur->ops->lookup(cur, name);
        if (!cur) return NULL;
    }
    return cur;
}

/* ── file descriptor table ───────────────────────────────────────────── */

#define MAX_FDS 16

typedef struct {
    vnode_t *vnode;
    uint32_t offset;
    int      flags;
    int      open;
} file_t;

static file_t fds[MAX_FDS];

static int fd_alloc(vnode_t *v, int flags) {
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fds[i].open) {
            fds[i].vnode  = v;
            fds[i].offset = 0;
            fds[i].flags  = flags;
            fds[i].open   = 1;
            return i;
        }
    }
    return -1;
}

/* ── public API ──────────────────────────────────────────────────────── */

void vfs_init(void) {
    /* Pre-open stdin(0)/stdout(1)/stderr(2). */
    vnode_t *kbd = vfs_lookup("/dev/kbd");
    vnode_t *vga = vfs_lookup("/dev/vga");

    if (kbd) fd_alloc(kbd, O_RDONLY);   /* fd 0 */
    if (vga) fd_alloc(vga, O_WRONLY);   /* fd 1 */
    if (vga) fd_alloc(vga, O_WRONLY);   /* fd 2 */
}

int vfs_open(const char *path, int flags) {
    vnode_t *v = vfs_lookup(path);
    if (!v) return -1;

    if (v->ops && v->ops->open) {
        int r = v->ops->open(v, flags);
        if (r < 0) return -1;
    }
    return fd_alloc(v, flags);
}

int vfs_read(int fd, void *buf, uint32_t len) {
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].open) return -1;
    file_t  *f = &fds[fd];
    fs_ops_t *ops = f->vnode->ops;
    if (!ops || !ops->read) return -1;
    int n = ops->read(f->vnode, buf, f->offset, len);
    if (n > 0 && f->vnode->type == VTYPE_FILE) f->offset += (uint32_t)n;
    return n;
}

int vfs_write(int fd, const void *buf, uint32_t len) {
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].open) return -1;
    file_t  *f = &fds[fd];
    fs_ops_t *ops = f->vnode->ops;
    if (!ops || !ops->write) return -1;
    int n = ops->write(f->vnode, buf, f->offset, len);
    if (n > 0 && f->vnode->type == VTYPE_FILE) f->offset += (uint32_t)n;
    return n;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].open) return -1;
    file_t *f = &fds[fd];
    if (f->vnode->ops && f->vnode->ops->close)
        f->vnode->ops->close(f->vnode);
    f->open = 0;
    return 0;
}

int vfs_readdir(int fd, uint32_t idx, char *name_out, uint32_t nmax) {
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].open) return -1;
    file_t  *f = &fds[fd];
    fs_ops_t *ops = f->vnode->ops;
    if (!ops || !ops->readdir) return -1;
    return ops->readdir(f->vnode, idx, name_out, nmax);
}

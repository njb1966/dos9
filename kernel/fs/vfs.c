#include <vfs.h>
#include <process.h>
#include <string.h>
#include <stddef.h>

/* ── mount table ─────────────────────────────────────────────────────── */

#define MAX_MOUNTS     8
#define MOUNT_PATH_MAX 32

typedef struct {
    char     path[MOUNT_PATH_MAX];  /* absolute, no trailing slash (except "/") */
    vnode_t *root;
    int      used;
} mount_t;

static mount_t mounts[MAX_MOUNTS];

/* Does `mount` (length mlen) match the start of `path` on a path boundary? */
static int prefix_matches(const char *mount, const char *path, uint32_t mlen) {
    for (uint32_t i = 0; i < mlen; i++)
        if (mount[i] != path[i]) return 0;
    if (path[mlen] == '\0')     return 1;   /* exact match */
    if (mount[mlen - 1] == '/') return 1;   /* mount is "/" — boundary implicit */
    return path[mlen] == '/';
}

/* Find the longest mount whose path covers `path`.  Returns the mount and
   sets `*remainder_out` to the portion of `path` after the mount prefix,
   with any leading slash stripped. */
static mount_t *find_mount(const char *path, const char **remainder_out) {
    mount_t *best = NULL;
    uint32_t best_len = 0;
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!mounts[i].used) continue;
        uint32_t mlen = strlen(mounts[i].path);
        if (!prefix_matches(mounts[i].path, path, mlen)) continue;
        if (mlen >= best_len) {
            best = &mounts[i];
            best_len = mlen;
        }
    }
    if (!best) return NULL;
    const char *rem = path + best_len;
    if (*rem == '/') rem++;
    *remainder_out = rem;
    return best;
}

/* True if `s` contains a '/'. */
static int has_slash(const char *s) {
    for (; *s; s++) if (*s == '/') return 1;
    return 0;
}

/* ── synthetic root vnode ────────────────────────────────────────────── */
/* The "/" mount is a directory whose children are the depth-1 mount
   points (e.g. /dev, /proc).  Lookup and readdir both consult the
   mount table directly, so adding a new mount makes it immediately
   visible at the root. */

static vnode_t *root_lookup(vnode_t *dir, const char *name) {
    (void)dir;
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!mounts[i].used) continue;
        if (mounts[i].path[0] != '/' || mounts[i].path[1] == '\0') continue;
        if (has_slash(mounts[i].path + 1)) continue;        /* not depth-1 */
        if (strcmp(mounts[i].path + 1, name) == 0) return mounts[i].root;
    }
    return NULL;
}

static int root_readdir(vnode_t *dir, uint32_t idx,
                         char *name_out, uint32_t nmax) {
    (void)dir;
    uint32_t seen = 0;
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!mounts[i].used) continue;
        if (mounts[i].path[0] != '/' || mounts[i].path[1] == '\0') continue;
        if (has_slash(mounts[i].path + 1)) continue;
        if (seen == idx) {
            strncpy(name_out, mounts[i].path + 1, nmax);
            return 0;
        }
        seen++;
    }
    return -1;
}

static fs_ops_t root_ops = {
    .lookup  = root_lookup,
    .readdir = root_readdir,
};

static vnode_t root_vnode = {
    .type = VTYPE_DIR, .size = 0, .priv = NULL, .ops = &root_ops,
};

/* ── path lookup ─────────────────────────────────────────────────────── */

vnode_t *vfs_lookup(const char *path) {
    if (!path || path[0] != '/') return NULL;

    const char *rem;
    mount_t *m = find_mount(path, &rem);
    if (!m) return NULL;

    vnode_t *cur = m->root;
    while (*rem) {
        char name[128];
        uint32_t i = 0;
        while (*rem && *rem != '/' && i < sizeof(name) - 1)
            name[i++] = *rem++;
        name[i] = '\0';
        if (*rem == '/') rem++;
        if (!name[0]) continue;             /* skip empty (trailing slash) */

        if (!cur->ops || !cur->ops->lookup) return NULL;
        cur = cur->ops->lookup(cur, name);
        if (!cur) return NULL;
    }
    return cur;
}

/* ── mount API ───────────────────────────────────────────────────────── */

int vfs_mount(const char *path, vnode_t *root) {
    if (!path || path[0] != '/' || !root) return -1;
    uint32_t len = strlen(path);
    if (len == 0 || len >= MOUNT_PATH_MAX) return -1;
    if (len > 1 && path[len - 1] == '/')   return -1;   /* no trailing slash */

    /* Replace existing mount at the same path, or take first free slot. */
    int slot = -1;
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mounts[i].used && strcmp(mounts[i].path, path) == 0) {
            mounts[i].root = root;
            return 0;
        }
        if (!mounts[i].used && slot < 0) slot = i;
    }
    if (slot < 0) return -1;

    strncpy(mounts[slot].path, path, MOUNT_PATH_MAX);
    mounts[slot].path[MOUNT_PATH_MAX - 1] = '\0';
    mounts[slot].root = root;
    mounts[slot].used = 1;
    return 0;
}

/* ── fd table helpers ─────────────────────────────────────────────────── */

/* Allocate a free fd in `table` for vnode `v`.  Returns fd index or -1. */
static int fd_alloc(file_t *table, vnode_t *v, int flags) {
    for (int i = 0; i < MAX_FDS; i++) {
        if (!table[i].open) {
            table[i].vnode  = v;
            table[i].offset = 0;
            table[i].flags  = flags;
            table[i].open   = 1;
            v->refs++;
            return i;
        }
    }
    return -1;
}

/* Release one reference to f's vnode; call ops->close only when refs hits 0. */
static void fd_release(file_t *f) {
    if (!f->open || !f->vnode) return;
    if (f->vnode->refs > 0) f->vnode->refs--;
    if (f->vnode->refs == 0 && f->vnode->ops && f->vnode->ops->close)
        f->vnode->ops->close(f->vnode);
    f->open = 0;
}

/* ── public API ──────────────────────────────────────────────────────── */

void vfs_init(void) {
    for (int i = 0; i < MAX_MOUNTS; i++) mounts[i].used = 0;
    vfs_mount("/", &root_vnode);
    /* per-process fd tables are zeroed in process_init/process_create */
}

void vfs_open_stdio(void) {
    file_t  *table = process_current_fds();
    vnode_t *kbd   = vfs_lookup("/dev/kbd");
    vnode_t *vga   = vfs_lookup("/dev/vga");
    if (kbd) fd_alloc(table, kbd, O_RDONLY);   /* fd 0 — stdin  */
    if (vga) fd_alloc(table, vga, O_WRONLY);   /* fd 1 — stdout */
    if (vga) fd_alloc(table, vga, O_WRONLY);   /* fd 2 — stderr */
}

int vfs_open(const char *path, int flags) {
    vnode_t *v = vfs_lookup(path);
    if (!v) return -1;

    if (v->ops && v->ops->open) {
        int r = v->ops->open(v, flags);
        if (r < 0) return -1;
    }
    return fd_alloc(process_current_fds(), v, flags);
}

int vfs_read(int fd, void *buf, uint32_t len) {
    if (fd < 0 || fd >= MAX_FDS) return -1;
    file_t  *f   = &process_current_fds()[fd];
    if (!f->open) return -1;
    fs_ops_t *ops = f->vnode->ops;
    if (!ops || !ops->read) return -1;
    int n = ops->read(f->vnode, buf, f->offset, len);
    if (n > 0 && f->vnode->type == VTYPE_FILE) f->offset += (uint32_t)n;
    return n;
}

int vfs_write(int fd, const void *buf, uint32_t len) {
    if (fd < 0 || fd >= MAX_FDS) return -1;
    file_t  *f   = &process_current_fds()[fd];
    if (!f->open) return -1;
    fs_ops_t *ops = f->vnode->ops;
    if (!ops || !ops->write) return -1;
    int n = ops->write(f->vnode, buf, f->offset, len);
    if (n > 0 && f->vnode->type == VTYPE_FILE) f->offset += (uint32_t)n;
    return n;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_FDS) return -1;
    file_t *f = &process_current_fds()[fd];
    if (!f->open) return -1;
    fd_release(f);
    return 0;
}

int vfs_dup(int oldfd) {
    file_t *fds = process_current_fds();
    if (oldfd < 0 || oldfd >= MAX_FDS || !fds[oldfd].open) return -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fds[i].open) {
            fds[i] = fds[oldfd];
            fds[i].vnode->refs++;
            return i;
        }
    }
    return -1;
}

int vfs_dup2(int oldfd, int newfd) {
    file_t *fds = process_current_fds();
    if (oldfd < 0 || oldfd >= MAX_FDS || !fds[oldfd].open) return -1;
    if (newfd < 0 || newfd >= MAX_FDS) return -1;
    if (oldfd == newfd) return newfd;
    if (fds[newfd].open) fd_release(&fds[newfd]);
    fds[newfd] = fds[oldfd];
    fds[newfd].vnode->refs++;
    return newfd;
}

int vfs_open_vnode(vnode_t *v, int flags) {
    if (!v) return -1;
    return fd_alloc(process_current_fds(), v, flags);
}

int vfs_lseek(int fd, int32_t offset, int whence) {
    if (fd < 0 || fd >= MAX_FDS) return -1;
    file_t *f = &process_current_fds()[fd];
    if (!f->open) return -1;
    if (f->vnode->type == VTYPE_CHR) return -1;    /* character devices not seekable */

    int32_t new_off;
    switch (whence) {
    case SEEK_SET: new_off = offset;                                 break;
    case SEEK_CUR: new_off = (int32_t)f->offset + offset;           break;
    case SEEK_END: new_off = (int32_t)f->vnode->size + offset;      break;
    default: return -1;
    }
    if (new_off < 0) return -1;
    f->offset = (uint32_t)new_off;
    return new_off;
}

int vfs_readdir(int fd, uint32_t idx, char *name_out, uint32_t nmax) {
    if (fd < 0 || fd >= MAX_FDS) return -1;
    file_t  *f   = &process_current_fds()[fd];
    if (!f->open) return -1;
    fs_ops_t *ops = f->vnode->ops;
    if (!ops || !ops->readdir) return -1;
    return ops->readdir(f->vnode, idx, name_out, nmax);
}

int vfs_unlink(const char *path) {
    if (!path || path[0] != '/') return -1;

    /* Find last '/' to split parent from name. */
    const char *last_slash = path;
    for (const char *p = path; *p; p++) if (*p == '/') last_slash = p;
    if (!last_slash[1]) return -1;                  /* trailing slash → no name */

    /* Extract parent path; "/foo" → "/", "/proc/3" → "/proc". */
    char parent[64];
    uint32_t plen = (uint32_t)(last_slash - path);
    if (plen == 0) { parent[0] = '/'; parent[1] = '\0'; }
    else {
        if (plen >= sizeof(parent)) return -1;
        for (uint32_t i = 0; i < plen; i++) parent[i] = path[i];
        parent[plen] = '\0';
    }

    vnode_t *dir = vfs_lookup(parent);
    if (!dir || !dir->ops || !dir->ops->unlink) return -1;
    return dir->ops->unlink(dir, last_slash + 1);
}

void vfs_close_table(file_t *table) {
    for (int i = 0; i < MAX_FDS; i++) {
        if (!table[i].open) continue;
        fd_release(&table[i]);
    }
}

void vfs_inherit_stdio(const file_t *src, file_t *dst) {
    for (int i = 0; i < 3; i++) {
        dst[i] = src[i];
        if (src[i].open && src[i].vnode)
            src[i].vnode->refs++;   /* child holds a new reference */
    }
}

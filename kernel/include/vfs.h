#pragma once
#include <stdint.h>

/* open() flags */
#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_CREAT   4

/* vnode types */
#define VTYPE_FILE 0
#define VTYPE_DIR  1
#define VTYPE_CHR  2    /* character device */

struct vnode;

/*
 * Filesystem operations.  Any pointer may be NULL if the operation is
 * not supported; VFS will return -1 in that case.
 */
typedef struct fs_ops {
    int            (*open)(struct vnode *v, int flags);
    int            (*close)(struct vnode *v);
    int            (*read)(struct vnode *v, void *buf, uint32_t off, uint32_t len);
    int            (*write)(struct vnode *v, const void *buf, uint32_t off, uint32_t len);
    struct vnode * (*lookup)(struct vnode *dir, const char *name);
    int            (*readdir)(struct vnode *dir, uint32_t idx,
                              char *name_out, uint32_t nmax);
    /* Remove a name from a directory.  Synthetic FSes may use this for
       side effects (e.g. /proc unlink kills the process). */
    int            (*unlink)(struct vnode *dir, const char *name);
    /* Create a new file in a directory; returns its vnode, or NULL on error. */
    struct vnode * (*create)(struct vnode *dir, const char *name);
} fs_ops_t;

typedef struct vnode {
    uint8_t    type;   /* VTYPE_* */
    uint32_t   size;   /* file size; 0 for dirs and char devs */
    void      *priv;   /* filesystem-private data */
    fs_ops_t  *ops;
    uint32_t   refs;   /* reference count — close when refs reaches 0 */
} vnode_t;

/* ── Per-process file descriptor table ─────────────────────────────────── */

#define MAX_FDS 16

typedef struct {
    vnode_t *vnode;
    uint32_t offset;
    int      flags;
    int      open;
} file_t;

/* lseek whence constants */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* ── VFS API ────────────────────────────────────────────────────────────── */

/* Initialise VFS: empty mount table, synthetic root at "/". */
void vfs_init(void);

/* Bind a filesystem's root vnode at an absolute path. */
int vfs_mount(const char *path, vnode_t *root);

/* Pre-open stdin/stdout/stderr in the current process's fd table.
   Call after devfs has mounted at /dev. */
void vfs_open_stdio(void);

/* Raw vnode lookup — returns NULL if path does not exist. */
vnode_t *vfs_lookup(const char *path);

/* File-descriptor API — return fd (≥0) on success, -1 on error. */
int vfs_open(const char *path, int flags);
int vfs_read(int fd, void *buf, uint32_t len);
int vfs_write(int fd, const void *buf, uint32_t len);
int vfs_close(int fd);
int vfs_lseek(int fd, int32_t offset, int whence);
int vfs_readdir(int fd, uint32_t idx, char *name_out, uint32_t nmax);

/* Remove a name from its parent directory. */
int vfs_unlink(const char *path);

/* Duplicate fds — reference-count aware. */
int vfs_dup(int oldfd);               /* dup to next free slot */
int vfs_dup2(int oldfd, int newfd);   /* dup to a specific slot */

/* Open a vnode directly into the current process's fd table (used by pipes). */
int vfs_open_vnode(vnode_t *v, int flags);

/* Per-process fd table helpers — called by process.c. */
void vfs_close_table(file_t *table);
void vfs_inherit_stdio(const file_t *src, file_t *dst);

#pragma once
#include <stdint.h>

/* open() flags */
#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2

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
    /* Remove a name from a directory.  For synthetic FSes the action may
       have side effects beyond filesystem state — e.g. /proc unlink kills
       the process.  Return 0 on success, -1 on error. */
    int            (*unlink)(struct vnode *dir, const char *name);
} fs_ops_t;

typedef struct vnode {
    uint8_t    type;   /* VTYPE_* */
    uint32_t   size;   /* file size; 0 for dirs and char devs */
    void      *priv;   /* filesystem-private data */
    fs_ops_t  *ops;
} vnode_t;

/* Initialise VFS: empty mount table, then mount the synthetic root at "/".
   Must be called before any vfs_mount() or vfs_lookup(). */
void vfs_init(void);

/* Bind a filesystem's root vnode at an absolute path.
   `path` must begin with '/', have no trailing slash (except "/" itself),
   and remain valid for the lifetime of the mount.  `root` must outlive
   the mount.  If a mount already exists at `path`, its root is replaced.
   Returns 0 on success, -1 on error (bad args or table full). */
int vfs_mount(const char *path, vnode_t *root);

/* Pre-open stdin (fd 0 → /dev/kbd) and stdout/stderr (fds 1,2 → /dev/vga).
   Call after devfs has mounted at /dev. */
void vfs_open_stdio(void);

/* Raw vnode lookup — returns NULL if path does not exist. */
vnode_t *vfs_lookup(const char *path);

/* File-descriptor API — return fd (≥0) on success, -1 on error. */
int vfs_open(const char *path, int flags);
int vfs_read(int fd, void *buf, uint32_t len);
int vfs_write(int fd, const void *buf, uint32_t len);
int vfs_close(int fd);
int vfs_readdir(int fd, uint32_t idx, char *name_out, uint32_t nmax);

/* Remove a name from its parent directory.  Returns 0 on success, -1 on
   error or if the parent's filesystem doesn't implement unlink. */
int vfs_unlink(const char *path);

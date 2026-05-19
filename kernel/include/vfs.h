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
} fs_ops_t;

typedef struct vnode {
    uint8_t    type;   /* VTYPE_* */
    uint32_t   size;   /* file size; 0 for dirs and char devs */
    void      *priv;   /* filesystem-private data */
    fs_ops_t  *ops;
} vnode_t;

/* Initialise VFS and pre-open stdin/stdout/stderr. */
void vfs_init(void);

/* Raw vnode lookup — returns NULL if path does not exist. */
vnode_t *vfs_lookup(const char *path);

/* File-descriptor API — return fd (≥0) on success, -1 on error. */
int vfs_open(const char *path, int flags);
int vfs_read(int fd, void *buf, uint32_t len);
int vfs_write(int fd, const void *buf, uint32_t len);
int vfs_close(int fd);
int vfs_readdir(int fd, uint32_t idx, char *name_out, uint32_t nmax);

#include <pipe.h>
#include <vfs.h>
#include <process.h>
#include <kheap.h>
#include <string.h>
#include <stdint.h>

#define PIPE_BUF 4096u

typedef struct pipe {
    uint8_t  data[PIPE_BUF];
    uint32_t head;       /* write index */
    uint32_t tail;       /* read index  */
    uint32_t open_ends;  /* 2 at creation; pipe_t freed when this hits 0 */
} pipe_t;

/*
 * Each pipe vnode's priv points to a small two-pointer array:
 *   priv[0] = pipe_t*
 *   priv[1] = the OTHER vnode* (kept for symmetry, not dereferenced by I/O)
 *
 * EOF is tracked through pipe_t::open_ends so the reader does not need to
 * touch the peer vnode after it has been closed and freed.
 */

static int pipe_read_op(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    (void)off;
    void   **priv = (void **)v->priv;
    pipe_t  *p    = (pipe_t *)priv[0];
    uint8_t *dst  = (uint8_t *)buf;
    uint32_t n    = 0;

    while (n < len) {
        while (p->head == p->tail) {          /* buffer empty */
            if (p->open_ends < 2) return (int)n; /* all writers closed → EOF */
            schedule();
        }
        dst[n++]  = p->data[p->tail];
        p->tail   = (p->tail + 1) % PIPE_BUF;
    }
    return (int)n;
}

static int pipe_write_op(vnode_t *v, const void *buf, uint32_t off, uint32_t len) {
    (void)off;
    void         **priv = (void **)v->priv;
    pipe_t        *p    = (pipe_t *)priv[0];
    const uint8_t *src  = (const uint8_t *)buf;
    uint32_t       n    = 0;

    while (n < len) {
        if (p->open_ends < 2) return -1;
        uint32_t next = (p->head + 1) % PIPE_BUF;
        while (next == p->tail) {             /* buffer full — yield until drained */
            if (p->open_ends < 2) return -1;
            schedule();
            next = (p->head + 1) % PIPE_BUF;
        }
        p->data[p->head] = src[n++];
        p->head = next;
    }
    return (int)n;
}

/*
 * pipe_close_op — called by fd_release when vnode->refs drops to 0.
 *
 * Frees this vnode and its priv array.  Decrements the shared open_ends
 * counter; when it reaches 0 (last end closing), frees the pipe_t buffer.
 * The peer vnode is freed by its own close call — no cross-vnode access.
 */
static int pipe_close_op(vnode_t *v) {
    void   **priv = (void **)v->priv;
    pipe_t  *p    = (pipe_t *)priv[0];   /* save before freeing priv */
    kfree(priv);
    kfree(v);
    if (--p->open_ends == 0)
        kfree(p);
    return 0;
}

static fs_ops_t pipe_read_ops  = { .read  = pipe_read_op,  .close = pipe_close_op };
static fs_ops_t pipe_write_ops = { .write = pipe_write_op, .close = pipe_close_op };

int pipe_create(int fds[2]) {
    pipe_t  *p  = (pipe_t *)kmalloc(sizeof(pipe_t));
    vnode_t *rv = (vnode_t *)kmalloc(sizeof(vnode_t));
    vnode_t *wv = (vnode_t *)kmalloc(sizeof(vnode_t));
    void   **rp = (void **)kmalloc(2 * sizeof(void *));
    void   **wp = (void **)kmalloc(2 * sizeof(void *));

    if (!p || !rv || !wv || !rp || !wp) {
        if (p)  kfree(p);
        if (rv) kfree(rv);
        if (wv) kfree(wv);
        if (rp) kfree(rp);
        if (wp) kfree(wp);
        return -1;
    }

    memset(p,  0, sizeof(pipe_t));
    memset(rv, 0, sizeof(vnode_t));
    memset(wv, 0, sizeof(vnode_t));
    p->open_ends = 2;

    /* Read vnode: can read, checks write_vnode->refs for EOF. */
    rp[0] = p;  rp[1] = wv;
    rv->type = VTYPE_CHR;
    rv->priv = rp;
    rv->ops  = &pipe_read_ops;

    /* Write vnode: can write. */
    wp[0] = p;  wp[1] = rv;
    wv->type = VTYPE_CHR;
    wv->priv = wp;
    wv->ops  = &pipe_write_ops;

    fds[0] = vfs_open_vnode(rv, O_RDONLY);
    if (fds[0] < 0) {
        kfree(p); kfree(rv); kfree(wv); kfree(rp); kfree(wp);
        return -1;
    }

    fds[1] = vfs_open_vnode(wv, O_WRONLY);
    if (fds[1] < 0) {
        vfs_close(fds[0]);   /* frees rv + rp through pipe_close_op */
        kfree(wv);
        kfree(wp);
        kfree(p);
        return -1;
    }
    return 0;
}

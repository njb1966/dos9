#include <diskfs.h>
#include <ata.h>
#include <vfs.h>
#include <string.h>
#include <terminal.h>
#include <stdint.h>
#include <stddef.h>

#define SECTOR_SIZE 512u

/* In-memory copy of the directory, populated at init time. */
static diskfs_dirent_t dir[DISKFS_MAX_FILES];
static uint32_t        n_active = 0;

/* One vnode per directory slot (used only for active entries). */
static vnode_t file_vnodes[DISKFS_MAX_FILES];

/* ── file read ─────────────────────────────────────────────────────────── */

static int diskfile_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    diskfs_dirent_t *de = (diskfs_dirent_t *)v->priv;
    if (off >= de->size) return 0;
    if (off + len > de->size) len = de->size - off;

    uint8_t  *dst       = (uint8_t *)buf;
    uint32_t  remaining = len;
    uint32_t  cur_off   = off;
    uint8_t   sector_buf[SECTOR_SIZE];

    while (remaining > 0) {
        uint32_t sector_idx = cur_off / SECTOR_SIZE;
        uint32_t sector_off = cur_off % SECTOR_SIZE;
        uint32_t lba        = de->start_lba + sector_idx;

        if (ata_read_sector(lba, sector_buf) < 0) return -1;

        uint32_t to_copy = SECTOR_SIZE - sector_off;
        if (to_copy > remaining) to_copy = remaining;

        memcpy(dst, sector_buf + sector_off, to_copy);
        dst       += to_copy;
        cur_off   += to_copy;
        remaining -= to_copy;
    }
    return (int)len;
}

static fs_ops_t diskfile_ops = { .read = diskfile_read };

/* ── directory ops ─────────────────────────────────────────────────────── */

static vnode_t *diskdir_lookup(vnode_t *d, const char *name) {
    (void)d;
    for (uint32_t i = 0; i < DISKFS_MAX_FILES; i++) {
        if (!(dir[i].flags & DISKFS_FLAG_ACTIVE)) continue;
        if (strcmp(dir[i].name, name) == 0) return &file_vnodes[i];
    }
    return NULL;
}

static int diskdir_readdir(vnode_t *d, uint32_t idx,
                            char *name_out, uint32_t nmax) {
    (void)d;
    uint32_t seen = 0;
    for (uint32_t i = 0; i < DISKFS_MAX_FILES; i++) {
        if (!(dir[i].flags & DISKFS_FLAG_ACTIVE)) continue;
        if (seen == idx) {
            strncpy(name_out, dir[i].name, nmax);
            return 0;
        }
        seen++;
    }
    return -1;
}

static fs_ops_t diskdir_ops = {
    .lookup  = diskdir_lookup,
    .readdir = diskdir_readdir,
};

static vnode_t diskdir_vnode = {
    .type = VTYPE_DIR, .size = 0, .priv = NULL, .ops = &diskdir_ops,
};

/* ── init ──────────────────────────────────────────────────────────────── */

void diskfs_init(void) {
    uint8_t sector_buf[SECTOR_SIZE];

    /* Read and validate the superblock. */
    if (ata_read_sector(0, sector_buf) < 0) {
        terminal_write("[DISKFS] sector read failed\n");
        return;
    }

    diskfs_super_t *sb = (diskfs_super_t *)sector_buf;
    if (sb->magic != DISKFS_MAGIC) {
        terminal_write("[DISKFS] no DOS9FS signature — disk not formatted\n");
        return;
    }

    /* Read directory sectors (1–4). */
    uint8_t dir_buf[4 * SECTOR_SIZE];
    for (int s = 0; s < 4; s++) {
        if (ata_read_sector(DISKFS_DIR_START + (uint32_t)s,
                            dir_buf + s * SECTOR_SIZE) < 0) {
            terminal_write("[DISKFS] directory read failed\n");
            return;
        }
    }

    memcpy(dir, dir_buf, sizeof(dir));

    /* Build vnodes for active entries. */
    n_active = 0;
    for (uint32_t i = 0; i < DISKFS_MAX_FILES; i++) {
        if (!(dir[i].flags & DISKFS_FLAG_ACTIVE)) continue;
        file_vnodes[i].type = VTYPE_FILE;
        file_vnodes[i].size = dir[i].size;
        file_vnodes[i].priv = &dir[i];
        file_vnodes[i].ops  = &diskfile_ops;
        n_active++;
    }

    vfs_mount("/disk", &diskdir_vnode);

    terminal_write("[DISKFS] mounted at /disk (");
    terminal_writedec(n_active);
    terminal_write(" files)\n");
}

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
static diskfs_super_t  super;

/* One vnode per directory slot (used only for active entries). */
static vnode_t file_vnodes[DISKFS_MAX_FILES];

/* LBA after last allocated file data. */
static uint32_t next_free_lba;

/* ── directory flush ───────────────────────────────────────────────────── */

static void diskfs_flush_dir(void) {
    uint8_t dir_buf[SECTOR_SIZE];
    for (int s = 0; s < 4; s++) {
        memset(dir_buf, 0, SECTOR_SIZE);
        memcpy(dir_buf, &dir[s * 16], 16 * sizeof(diskfs_dirent_t));
        ata_write_sector(DISKFS_DIR_START + (uint32_t)s, dir_buf);
    }
}

static void diskfs_flush_super(void) {
    uint8_t sb_buf[SECTOR_SIZE];
    memset(sb_buf, 0, SECTOR_SIZE);
    memcpy(sb_buf, &super, sizeof(super));
    ata_write_sector(0, sb_buf);
}

/* ── file read ─────────────────────────────────────────────────────────── */

static int diskfile_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    diskfs_dirent_t *de = (diskfs_dirent_t *)v->priv;
    if (off >= de->size) return 0;
    if (len > de->size - off) len = de->size - off;

    uint8_t  *dst       = (uint8_t *)buf;
    uint32_t  remaining = len;
    uint32_t  cur_off   = off;
    uint8_t   sector_buf[SECTOR_SIZE];

    while (remaining > 0) {
        uint32_t sector_idx = cur_off / SECTOR_SIZE;
        uint32_t sector_off = cur_off % SECTOR_SIZE;
        uint64_t lba64      = (uint64_t)de->start_lba + (uint64_t)sector_idx;
        if (lba64 > 0xFFFFFFFFu) return -1;
        uint32_t lba        = (uint32_t)lba64;

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

/* ── file write ────────────────────────────────────────────────────────── */

static int diskfile_write(vnode_t *v, const void *buf,
                           uint32_t off, uint32_t len) {
    diskfs_dirent_t *de = (diskfs_dirent_t *)v->priv;
    uint64_t max_bytes64 = (uint64_t)de->alloc_sectors * (uint64_t)SECTOR_SIZE;
    uint32_t max_bytes   = (max_bytes64 > 0xFFFFFFFFu)
        ? 0xFFFFFFFFu
        : (uint32_t)max_bytes64;

    if (off >= max_bytes) return 0;
    if (len > max_bytes - off) len = max_bytes - off;
    if (len == 0) return 0;

    const uint8_t *src  = (const uint8_t *)buf;
    uint8_t  sector_buf[SECTOR_SIZE];

    /* If the caller seeks past EOF and then writes, zero-fill the hole so we
       do not expose stale bytes from the preallocated region. */
    if (off > de->size) {
        uint32_t zero_off = de->size;
        uint32_t zero_remaining = off - de->size;

        while (zero_remaining > 0) {
            uint32_t sector_idx = zero_off / SECTOR_SIZE;
            uint32_t sector_off = zero_off % SECTOR_SIZE;
            uint64_t lba64      = (uint64_t)de->start_lba + (uint64_t)sector_idx;
            if (lba64 > 0xFFFFFFFFu) return -1;
            uint32_t lba        = (uint32_t)lba64;

            if (sector_off != 0 || zero_remaining < SECTOR_SIZE) {
                if (ata_read_sector(lba, sector_buf) < 0) return -1;
            } else {
                memset(sector_buf, 0, SECTOR_SIZE);
            }

            uint32_t to_zero = SECTOR_SIZE - sector_off;
            if (to_zero > zero_remaining) to_zero = zero_remaining;
            memset(sector_buf + sector_off, 0, to_zero);

            if (ata_write_sector(lba, sector_buf) < 0) return -1;

            zero_off       += to_zero;
            zero_remaining -= to_zero;
        }
    }

    uint32_t remaining  = len;
    uint32_t cur_off    = off;

    while (remaining > 0) {
        uint32_t sector_idx = cur_off / SECTOR_SIZE;
        uint32_t sector_off = cur_off % SECTOR_SIZE;
        uint64_t lba64      = (uint64_t)de->start_lba + (uint64_t)sector_idx;
        if (lba64 > 0xFFFFFFFFu) return -1;
        uint32_t lba        = (uint32_t)lba64;

        if (sector_off != 0 || remaining < SECTOR_SIZE) {
            if (ata_read_sector(lba, sector_buf) < 0) return -1;
        } else {
            memset(sector_buf, 0, SECTOR_SIZE);
        }

        uint32_t to_copy = SECTOR_SIZE - sector_off;
        if (to_copy > remaining) to_copy = remaining;
        memcpy(sector_buf + sector_off, src, to_copy);

        if (ata_write_sector(lba, sector_buf) < 0) return -1;

        src       += to_copy;
        cur_off   += to_copy;
        remaining -= to_copy;
    }

    uint32_t new_end = off + len;
    de->size = new_end;
    v->size  = new_end;
    diskfs_flush_dir();
    return (int)len;
}

static fs_ops_t diskfile_ops = {
    .read  = diskfile_read,
    .write = diskfile_write,
};

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
            if (nmax == 0) return -1;
            uint32_t j = 0;
            while (j + 1 < nmax && dir[i].name[j]) {
                name_out[j] = dir[i].name[j];
                j++;
            }
            name_out[j] = '\0';
            return 0;
        }
        seen++;
    }
    return -1;
}

static vnode_t *diskdir_create(vnode_t *d, const char *name) {
    (void)d;
    if (!name || !name[0]) return NULL;

    uint32_t nlen = 0;
    while (name[nlen]) nlen++;
    if (nlen >= 16) return NULL;

    for (uint32_t i = 0; i < DISKFS_MAX_FILES; i++) {
        if ((dir[i].flags & DISKFS_FLAG_ACTIVE) &&
            strcmp(dir[i].name, name) == 0)
            return NULL;   /* already exists */
    }

    uint32_t slot = DISKFS_MAX_FILES;
    for (uint32_t i = 0; i < DISKFS_MAX_FILES; i++) {
        if (!(dir[i].flags & DISKFS_FLAG_ACTIVE)) { slot = i; break; }
    }
    if (slot == DISKFS_MAX_FILES) return NULL;
    if (next_free_lba > 0xFFFFFFFFu - DISKFS_PREALLOC_SECTORS) return NULL;

    for (uint32_t i = 0; i < nlen; i++) dir[slot].name[i] = name[i];
    dir[slot].name[nlen]      = '\0';
    dir[slot].start_lba       = next_free_lba;
    dir[slot].size            = 0;
    dir[slot].flags           = DISKFS_FLAG_ACTIVE;
    dir[slot].alloc_sectors   = DISKFS_PREALLOC_SECTORS;

    next_free_lba += DISKFS_PREALLOC_SECTORS;
    n_active++;
    super.n_files = n_active;

    file_vnodes[slot].type = VTYPE_FILE;
    file_vnodes[slot].size = 0;
    file_vnodes[slot].priv = &dir[slot];
    file_vnodes[slot].ops  = &diskfile_ops;
    file_vnodes[slot].refs = 0;

    diskfs_flush_dir();
    diskfs_flush_super();
    return &file_vnodes[slot];
}

static fs_ops_t diskdir_ops = {
    .lookup  = diskdir_lookup,
    .readdir = diskdir_readdir,
    .create  = diskdir_create,
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
    super = *sb;

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
        file_vnodes[i].refs = 0;
        n_active++;
    }

    /* Compute next_free_lba = first LBA past all allocated file data. */
    next_free_lba = DISKFS_DATA_START;
    for (uint32_t i = 0; i < DISKFS_MAX_FILES; i++) {
        if (!(dir[i].flags & DISKFS_FLAG_ACTIVE)) continue;
        uint64_t end64 = (uint64_t)dir[i].start_lba + (uint64_t)dir[i].alloc_sectors;
        uint32_t end = (end64 > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)end64;
        if (end > next_free_lba) next_free_lba = end;
    }
    super.n_files = n_active;
    super.data_start = DISKFS_DATA_START;
    diskfs_flush_super();

    vfs_mount("/disk", &diskdir_vnode);

    terminal_write("[DISKFS] mounted at /disk (");
    terminal_writedec(n_active);
    terminal_write(" files)\n");
}

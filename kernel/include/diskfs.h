#pragma once
#include <stdint.h>

/* ── DOS9FS on-disk layout ───────────────────────────────────────────────
 *
 *  Sector 0      : Superblock  (512 bytes)
 *  Sectors 1–4   : Directory   (64 entries × 32 bytes = 2048 bytes)
 *  Sectors 5+    : File data   (contiguous per file)
 *
 * All values are little-endian (native x86).
 */

#define DISKFS_MAGIC       0xD0590001u
#define DISKFS_DIR_START   1u     /* first directory sector */
#define DISKFS_DATA_START  5u     /* first file-data sector */
#define DISKFS_MAX_FILES   64u
#define DISKFS_SECTOR_SIZE 512u

#define DISKFS_FLAG_ACTIVE 1u

#define DISKFS_PREALLOC_SECTORS 64u   /* 32KB pre-allocated per new text file */

typedef struct {
    uint32_t magic;        /* DISKFS_MAGIC */
    uint32_t n_files;      /* number of active directory entries */
    uint32_t data_start;   /* always DISKFS_DATA_START */
    uint8_t  pad[500];
} __attribute__((packed)) diskfs_super_t;

typedef struct {
    char     name[16];     /* filename, null-terminated, no path */
    uint32_t start_lba;    /* first data sector */
    uint32_t size;         /* file size in bytes */
    uint32_t flags;        /* DISKFS_FLAG_ACTIVE or 0 (empty slot) */
    uint32_t alloc_sectors; /* sectors reserved for this file */
} __attribute__((packed)) diskfs_dirent_t;

/* Mount the disk filesystem at /disk.  Must be called after ata_init()
   confirms a drive is present.  Silently returns if the disk has no
   valid DOS9FS superblock. */
void diskfs_init(void);

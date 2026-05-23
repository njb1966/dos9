/*
 * mkdisk — DOS9FS disk image tool.
 *
 * Usage: mkdisk <disk.img> <diskname>=<hostfile> [<diskname>=<hostfile> ...]
 *
 * Writes a fresh DOS9FS superblock + directory to disk.img, then copies
 * each named file.  Existing file data beyond sector 5 is overwritten.
 * disk.img must already exist (create with: dd if=/dev/zero ... ).
 *
 * Compiled with native gcc (not the cross-compiler).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── On-disk layout (must match diskfs.h) ─────────────────────────────── */
#define DISKFS_MAGIC       0xD0590001u
#define DISKFS_DIR_START   1u
#define DISKFS_DATA_START  5u
#define DISKFS_MAX_FILES   64u
#define DISKFS_SECTOR_SIZE 512u
#define DISKFS_FLAG_ACTIVE 1u

typedef struct {
    uint32_t magic;
    uint32_t n_files;
    uint32_t data_start;
    uint8_t  pad[500];
} __attribute__((packed)) diskfs_super_t;

typedef struct {
    char     name[16];
    uint32_t start_lba;
    uint32_t size;
    uint32_t flags;
    uint32_t alloc_sectors;
} __attribute__((packed)) diskfs_dirent_t;

/* ── helpers ───────────────────────────────────────────────────────────── */

static void write_sector(FILE *f, uint32_t lba, const void *buf) {
    if (fseek(f, (long)(lba * DISKFS_SECTOR_SIZE), SEEK_SET) != 0) {
        perror("fseek"); exit(1);
    }
    if (fwrite(buf, DISKFS_SECTOR_SIZE, 1, f) != 1) {
        perror("fwrite"); exit(1);
    }
}

static uint32_t sectors_for(uint32_t bytes) {
    return (bytes + DISKFS_SECTOR_SIZE - 1) / DISKFS_SECTOR_SIZE;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: mkdisk <disk.img> [name=file ...]\n");
        return 1;
    }

    const char *img = argv[1];
    int n_args = argc - 2;

    if (n_args > (int)DISKFS_MAX_FILES) {
        fprintf(stderr, "error: too many files (max %u)\n", DISKFS_MAX_FILES);
        return 1;
    }

    FILE *f = fopen(img, "r+b");
    if (!f) { perror(img); return 1; }

    /* Build directory in memory. */
    diskfs_dirent_t dir[DISKFS_MAX_FILES];
    memset(dir, 0, sizeof(dir));

    uint32_t next_lba = DISKFS_DATA_START;
    uint32_t n_files  = 0;

    for (int i = 0; i < n_args; i++) {
        char *arg = argv[2 + i];
        char *eq  = strchr(arg, '=');
        if (!eq) {
            fprintf(stderr, "error: bad argument '%s' (expected name=file)\n", arg);
            fclose(f); return 1;
        }
        *eq = '\0';
        const char *disk_name = arg;
        const char *host_file = eq + 1;

        if (strlen(disk_name) >= 16) {
            fprintf(stderr, "error: name '%s' too long (max 15 chars)\n", disk_name);
            fclose(f); return 1;
        }

        /* Read the source file. */
        FILE *src = fopen(host_file, "rb");
        if (!src) { perror(host_file); fclose(f); return 1; }

        fseek(src, 0, SEEK_END);
        long sz = ftell(src);
        rewind(src);

        if (sz < 0 || sz > 64 * 1024 * 1024L) {
            fprintf(stderr, "error: '%s' too large\n", host_file);
            fclose(src); fclose(f); return 1;
        }

        uint32_t size     = (uint32_t)sz;
        uint32_t n_sects  = sectors_for(size);
        uint8_t  sector_buf[DISKFS_SECTOR_SIZE];

        /* Copy file data sector by sector. */
        uint32_t remaining = size;
        uint32_t lba       = next_lba;
        while (remaining > 0 || lba < next_lba + n_sects) {
            uint32_t to_read = remaining < DISKFS_SECTOR_SIZE
                               ? remaining : DISKFS_SECTOR_SIZE;
            memset(sector_buf, 0, DISKFS_SECTOR_SIZE);
            if (to_read > 0) {
                if (fread(sector_buf, 1, to_read, src) != to_read) {
                    perror(host_file); fclose(src); fclose(f); return 1;
                }
                remaining -= to_read;
            }
            write_sector(f, lba++, sector_buf);
        }
        fclose(src);

        /* Directory entry. */
        strncpy(dir[i].name, disk_name, 15);
        dir[i].name[15]      = '\0';
        dir[i].start_lba     = next_lba;
        dir[i].size          = size;
        dir[i].flags         = DISKFS_FLAG_ACTIVE;
        dir[i].alloc_sectors = n_sects;

        printf("  %-15s  lba=%-5u  %u bytes (%u sectors)\n",
               disk_name, next_lba, size, n_sects);

        next_lba += n_sects;
        n_files++;
    }

    /* Write superblock. */
    uint8_t sb_buf[DISKFS_SECTOR_SIZE];
    memset(sb_buf, 0, DISKFS_SECTOR_SIZE);
    diskfs_super_t *sb = (diskfs_super_t *)sb_buf;
    sb->magic      = DISKFS_MAGIC;
    sb->n_files    = n_files;
    sb->data_start = DISKFS_DATA_START;
    write_sector(f, 0, sb_buf);

    /* Write directory (sectors 1–4). */
    for (int s = 0; s < 4; s++) {
        uint8_t dir_buf[DISKFS_SECTOR_SIZE];
        memset(dir_buf, 0, DISKFS_SECTOR_SIZE);
        /* 16 entries of 32 bytes fit in one sector. */
        memcpy(dir_buf, &dir[s * 16], 16 * sizeof(diskfs_dirent_t));
        write_sector(f, DISKFS_DIR_START + (uint32_t)s, dir_buf);
    }

    fclose(f);

    printf("DOS9FS written to %s: %u file(s), data starts at sector %u\n",
           img, n_files, DISKFS_DATA_START);
    return 0;
}

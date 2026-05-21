/*
 * pkg — DOS/9 package installer.
 *
 * Usage:
 *   pkg install <path>   install a .d9p package from VFS path to /disk
 *   pkg info    <path>   show package metadata without installing
 *   pkg help             show usage
 *
 * Package header (.d9p, 64 bytes):
 *   magic[4]   "D9PK"
 *   version[2] 1
 *   name[32]   package name (null-terminated)
 *   elf_size[4] payload size in bytes
 *   crc32[4]   CRC32 of ELF payload
 *   pad[18]    reserved
 */

#include <dos9.h>

#define D9PK_HDR_SIZE 64

typedef struct {
    char     magic[4];
    uint16_t version;
    char     name[32];
    uint32_t elf_size;
    uint32_t crc32;
    uint8_t  pad[18];
} __attribute__((packed)) d9pk_hdr_t;

static uint32_t crc32_compute(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320u;
            else         crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

/* Read package header (and optionally payload) from VFS path.
   If elf_out is non-NULL, malloc's and returns the ELF payload.
   Caller must free *elf_out. */
static int read_pkg(const char *path, d9pk_hdr_t *hdr,
                    uint8_t **elf_out, uint32_t *size_out) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("pkg: cannot open '%s'\n", path);
        return -1;
    }

    /* Read 64-byte header. */
    uint8_t hdrbuf[D9PK_HDR_SIZE];
    int r = read(fd, hdrbuf, D9PK_HDR_SIZE);
    if (r != D9PK_HDR_SIZE) {
        printf("pkg: short header read (%d bytes)\n", r);
        close(fd);
        return -1;
    }
    memcpy(hdr, hdrbuf, sizeof(*hdr));

    if (hdr->magic[0] != 'D' || hdr->magic[1] != '9' ||
        hdr->magic[2] != 'P' || hdr->magic[3] != 'K') {
        puts("pkg: not a valid .d9p package (bad magic)");
        close(fd);
        return -1;
    }

    if (!elf_out || !size_out) {
        close(fd);
        return 0;
    }

    if (hdr->elf_size == 0 || hdr->elf_size > 512 * 1024u) {
        printf("pkg: implausible payload size (%u)\n", hdr->elf_size);
        close(fd);
        return -1;
    }

    uint8_t *buf = malloc(hdr->elf_size);
    if (!buf) {
        puts("pkg: out of memory");
        close(fd);
        return -1;
    }

    /* Read payload in chunks (disk read may return short). */
    uint32_t remaining = hdr->elf_size;
    uint8_t *ptr = buf;
    while (remaining > 0) {
        uint32_t chunk = remaining > 4096u ? 4096u : remaining;
        int n = read(fd, ptr, chunk);
        if (n <= 0) {
            puts("pkg: read error in payload");
            free(buf);
            close(fd);
            return -1;
        }
        ptr       += (uint32_t)n;
        remaining -= (uint32_t)n;
    }

    close(fd);
    *elf_out  = buf;
    *size_out = hdr->elf_size;
    return 0;
}

static int cmd_info(const char *path) {
    d9pk_hdr_t hdr;
    if (read_pkg(path, &hdr, NULL, NULL) < 0) return 1;

    printf("name:    %s\n", hdr.name);
    printf("version: %u\n", (unsigned)hdr.version);
    printf("size:    %u bytes\n", hdr.elf_size);
    printf("crc32:   %08x\n", hdr.crc32);
    return 0;
}

static int cmd_install(const char *path) {
    d9pk_hdr_t hdr;
    uint8_t   *elf_data = NULL;
    uint32_t   elf_size = 0;

    if (read_pkg(path, &hdr, &elf_data, &elf_size) < 0) return 1;

    /* Verify CRC32 before touching the disk. */
    uint32_t computed = crc32_compute(elf_data, elf_size);
    if (computed != hdr.crc32) {
        printf("pkg: CRC32 mismatch (expected %08x, got %08x)\n",
               hdr.crc32, computed);
        free(elf_data);
        return 1;
    }

    /* Build /disk/<name>. */
    char dest[24];
    dest[0] = '/'; dest[1] = 'd'; dest[2] = 'i';
    dest[3] = 's'; dest[4] = 'k'; dest[5] = '/';
    uint32_t i = 0;
    while (i < 15u && hdr.name[i]) { dest[6 + i] = hdr.name[i]; i++; }
    dest[6 + i] = '\0';

    /* Open existing file for overwrite, or create new. */
    int out = open(dest, O_RDWR);
    if (out < 0) out = open(dest, O_CREAT | O_WRONLY);
    if (out < 0) {
        printf("pkg: cannot create '%s'\n", dest);
        free(elf_data);
        return 1;
    }

    int n = write(out, elf_data, elf_size);
    close(out);
    free(elf_data);

    if (n < 0 || (uint32_t)n != elf_size) {
        printf("pkg: write incomplete (%d/%u bytes) — disk full?\n",
               n, elf_size);
        return 1;
    }

    printf("installed: %s → %s (%u bytes)\n", hdr.name, dest, elf_size);
    return 0;
}

int main(int argc, const char **argv) {
    if (argc < 2) {
        puts("usage: pkg <install|info|help> [path]");
        return 1;
    }

    if (strcmp(argv[1], "help") == 0) {
        puts("pkg -- DOS/9 package installer");
        puts("  pkg install <path>  install .d9p package to /disk/<name>");
        puts("  pkg info    <path>  show package name, version, size, crc32");
        puts("  pkg help            show this help");
        puts("note: install will fail if the target file has insufficient");
        puts("      pre-allocated sectors; remove the old file first.");
        return 0;
    }

    if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) { puts("pkg: install requires a path"); return 1; }
        return cmd_install(argv[2]);
    }

    if (strcmp(argv[1], "info") == 0) {
        if (argc < 3) { puts("pkg: info requires a path"); return 1; }
        return cmd_info(argv[2]);
    }

    printf("pkg: unknown command '%s'\n", argv[1]);
    return 1;
}

/*
 * pack — create a DOS/9 package (.d9p) from an ELF binary.
 *
 * Usage: ./tools/pack <name> <input.elf> <output.d9p>
 *
 * Package layout (64-byte header + ELF payload):
 *   magic[4]     "D9PK"
 *   version[2]   1
 *   name[32]     package name (null-terminated, ≤15 chars)
 *   elf_size[4]  payload length in bytes
 *   crc32[4]     CRC32 of the ELF payload
 *   pad[18]      reserved
 *
 * Compiled with native gcc (not the cross-compiler).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define D9PK_MAGIC   "D9PK"
#define D9PK_VERSION 1

typedef struct {
    char     magic[4];
    uint16_t version;
    char     name[32];
    uint32_t elf_size;
    uint32_t crc32;
    uint8_t  pad[18];
} __attribute__((packed)) d9pk_hdr_t;

_Static_assert(sizeof(d9pk_hdr_t) == 64, "d9pk_hdr_t must be 64 bytes");

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

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: pack <name> <input.elf> <output.d9p>\n");
        return 1;
    }

    const char *name    = argv[1];
    const char *inpath  = argv[2];
    const char *outpath = argv[3];

    if (strlen(name) > 15) {
        fprintf(stderr, "error: name '%s' too long (max 15 chars)\n", name);
        return 1;
    }

    FILE *in = fopen(inpath, "rb");
    if (!in) { perror(inpath); return 1; }

    fseek(in, 0, SEEK_END);
    long sz = ftell(in);
    rewind(in);

    if (sz <= 0 || sz > 32 * 1024 * 1024L) {
        fprintf(stderr, "error: '%s' size out of range\n", inpath);
        fclose(in);
        return 1;
    }

    uint32_t elf_size = (uint32_t)sz;
    uint8_t *elf_data = malloc(elf_size);
    if (!elf_data) {
        fprintf(stderr, "error: out of memory\n");
        fclose(in);
        return 1;
    }

    if (fread(elf_data, 1, elf_size, in) != elf_size) {
        perror(inpath);
        free(elf_data);
        fclose(in);
        return 1;
    }
    fclose(in);

    d9pk_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, D9PK_MAGIC, 4);
    hdr.version  = D9PK_VERSION;
    strncpy(hdr.name, name, sizeof(hdr.name) - 1);
    hdr.elf_size = elf_size;
    hdr.crc32    = crc32_compute(elf_data, elf_size);

    FILE *out = fopen(outpath, "wb");
    if (!out) {
        perror(outpath);
        free(elf_data);
        return 1;
    }

    if (fwrite(&hdr, sizeof(hdr), 1, out) != 1 ||
        fwrite(elf_data, 1, elf_size, out) != elf_size) {
        perror(outpath);
        free(elf_data);
        fclose(out);
        return 1;
    }

    fclose(out);
    free(elf_data);

    printf("packed: %-15s  %s  (%u bytes, crc32=%08x)\n",
           name, outpath, elf_size, hdr.crc32);
    return 0;
}

#pragma once
#include <stdint.h>

#define MULTIBOOT_MAGIC     0x2BADB002

#define MULTIBOOT_FLAG_MEM  (1u << 0)   /* mem_lower/mem_upper valid */
#define MULTIBOOT_FLAG_MODS (1u << 3)   /* mods_count/mods_addr valid */
#define MULTIBOOT_FLAG_MMAP (1u << 6)   /* mmap_addr/mmap_length valid */

#define MULTIBOOT_MMAP_AVAILABLE 1

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;     /* KB of low memory (below 1MB) */
    uint32_t mem_upper;     /* KB of upper memory (above 1MB) */
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed));

/* Multiboot module descriptor */
struct multiboot_mod {
    uint32_t mod_start;   /* physical address of module data */
    uint32_t mod_end;     /* physical address one past the end */
    uint32_t cmdline;     /* module command line string (physical addr) */
    uint32_t pad;
} __attribute__((packed));

/* Each entry is preceded by a uint32_t `size` field (not included in size). */
struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

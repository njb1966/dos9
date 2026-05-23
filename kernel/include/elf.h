#pragma once
#include <stdint.h>

/* ELF32 magic (little-endian "\x7FELF") */
#define ELF_MAGIC   0x464C457Fu
#define ET_EXEC     2
#define EM_386      3
#define PT_LOAD     1
#define ELF_PF_W    2   /* program header write flag */

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) Elf32_Phdr;

/*
 * Load an ELF executable from a memory buffer into a fresh user page
 * directory.  Maps each PT_LOAD segment, copies file data, zeroes BSS.
 *
 * Returns the entry point virtual address on success, 0 on failure.
 * *pd_out  receives the physical address of the new user page directory.
 * *brk_out receives the initial heap break address (page-aligned end of
 *          the highest PT_LOAD segment) — the starting point for sbrk().
 */
uint32_t elf_load(const void *data, uint32_t size,
                  uint32_t *pd_out, uint32_t *brk_out);

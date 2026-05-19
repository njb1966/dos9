#include <elf.h>
#include <vmm.h>
#include <pmm.h>
#include <kernel.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096u

/*
 * Walk the user page directory to find the physical frame backing vaddr,
 * then return the kernel-accessible address of that byte.
 */
static uint8_t *user_kaddr(uint32_t pd_phys, uint32_t vaddr) {
    uint32_t *pd      = (uint32_t *)VIRT(pd_phys);
    uint32_t  pd_idx  = vaddr >> 22;
    uint32_t  pt_idx  = (vaddr >> 12) & 0x3FFu;
    uint32_t  pg_off  = vaddr & 0xFFFu;

    if (!(pd[pd_idx] & PF_PRESENT)) return NULL;
    uint32_t *pt     = (uint32_t *)VIRT(pd[pd_idx] & ~0xFFFu);
    if (!(pt[pt_idx] & PF_PRESENT)) return NULL;
    return (uint8_t *)VIRT(pt[pt_idx] & ~0xFFFu) + pg_off;
}

uint32_t elf_load(const void *data, uint32_t size, uint32_t *pd_out) {
    const uint8_t   *bin  = (const uint8_t *)data;
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)bin;

    if (size < sizeof(Elf32_Ehdr)) return 0;

    uint32_t magic;
    memcpy(&magic, ehdr->e_ident, 4);
    if (magic != ELF_MAGIC)      return 0;
    if (ehdr->e_ident[4] != 1)   return 0;   /* ELFCLASS32 */
    if (ehdr->e_type    != ET_EXEC) return 0;
    if (ehdr->e_machine != EM_386)  return 0;

    uint32_t pd_phys = vmm_create_user_pd();

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        uint32_t ph_off = ehdr->e_phoff + (uint32_t)i * ehdr->e_phentsize;
        if (ph_off + sizeof(Elf32_Phdr) > size) continue;

        const Elf32_Phdr *phdr = (const Elf32_Phdr *)(bin + ph_off);
        if (phdr->p_type  != PT_LOAD)  continue;
        if (phdr->p_memsz == 0)        continue;
        if (phdr->p_vaddr >= 0xC0000000u) return 0;   /* must be user-space */

        uint32_t flags  = PF_PRESENT | PF_USER;
        if (phdr->p_flags & ELF_PF_W) flags |= PF_WRITE;

        /* Allocate and map pages covering the segment. */
        uint32_t vstart = phdr->p_vaddr & ~(PAGE_SIZE - 1);
        uint32_t vend   = (phdr->p_vaddr + phdr->p_memsz + PAGE_SIZE - 1)
                          & ~(PAGE_SIZE - 1);

        for (uint32_t va = vstart; va < vend; va += PAGE_SIZE) {
            uint32_t frame = (uint32_t)pmm_alloc_frame();
            memset((void *)VIRT(frame), 0, PAGE_SIZE);
            vmm_map_page_in(pd_phys, va, frame, flags);
        }

        /* Copy file data into the mapped pages, walking the PD. */
        for (uint32_t off = 0; off < phdr->p_filesz; off++) {
            uint8_t *dst = user_kaddr(pd_phys, phdr->p_vaddr + off);
            if (dst) *dst = bin[phdr->p_offset + off];
        }
    }

    *pd_out = pd_phys;
    return ehdr->e_entry;
}

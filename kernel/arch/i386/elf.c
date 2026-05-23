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

/*
 * Tear down the user half of a page directory created by elf_load().
 * The kernel half is shared, so only entries below KERNEL_PD_IDX are owned.
 */
static void destroy_user_pd(uint32_t pd_phys) {
    uint32_t *pd = (uint32_t *)VIRT(pd_phys);

    for (uint32_t pd_idx = 0; pd_idx < 768u; pd_idx++) {
        if (!(pd[pd_idx] & PF_PRESENT)) continue;

        uint32_t *pt = (uint32_t *)VIRT(pd[pd_idx] & ~0xFFFu);
        for (uint32_t pt_idx = 0; pt_idx < 1024u; pt_idx++) {
            if (!(pt[pt_idx] & PF_PRESENT)) continue;
            pmm_free_frame((void *)(uintptr_t)(pt[pt_idx] & ~0xFFFu));
        }

        pmm_free_frame((void *)(uintptr_t)(pd[pd_idx] & ~0xFFFu));
        pd[pd_idx] = 0;
    }

    pmm_free_frame((void *)(uintptr_t)pd_phys);
}

uint32_t elf_load(const void *data, uint32_t size,
                  uint32_t *pd_out, uint32_t *brk_out) {
    const uint8_t   *bin  = (const uint8_t *)data;
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)bin;

    if (size < sizeof(Elf32_Ehdr)) return 0;

    uint32_t magic;
    memcpy(&magic, ehdr->e_ident, 4);
    if (magic != ELF_MAGIC)      return 0;
    if (ehdr->e_ident[4] != 1)   return 0;   /* ELFCLASS32 */
    if (ehdr->e_type    != ET_EXEC) return 0;
    if (ehdr->e_machine != EM_386)  return 0;

    uint32_t pd_phys  = vmm_create_user_pd();
    uint32_t top_end  = 0;   /* highest vaddr + memsz seen across all PT_LOAD */
    if (!pd_phys) return 0;

    if (ehdr->e_phentsize != sizeof(Elf32_Phdr)) return 0;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        uint64_t ph_off64 = (uint64_t)ehdr->e_phoff +
                            (uint64_t)i * (uint64_t)ehdr->e_phentsize;
        if (ph_off64 > size) continue;
        if ((uint64_t)sizeof(Elf32_Phdr) > (uint64_t)size - ph_off64) continue;
        uint32_t ph_off = (uint32_t)ph_off64;

        const Elf32_Phdr *phdr = (const Elf32_Phdr *)(bin + ph_off);
        if (phdr->p_type  != PT_LOAD)  continue;
        if (phdr->p_memsz == 0)        continue;
        if (phdr->p_filesz > phdr->p_memsz) return 0;
        if (phdr->p_offset > size) return 0;
        if (phdr->p_filesz > size - phdr->p_offset) return 0;
        if (phdr->p_vaddr >= 0xC0000000u) return 0;   /* must be user-space */

        uint32_t flags  = PF_PRESENT | PF_USER;
        if (phdr->p_flags & ELF_PF_W) flags |= PF_WRITE;

        /* Allocate and map pages covering the segment. */
        uint32_t vstart = phdr->p_vaddr & ~(PAGE_SIZE - 1);
        uint32_t seg_end = phdr->p_vaddr + phdr->p_memsz;
        if (seg_end < phdr->p_vaddr) return 0;
        if (seg_end > 0xC0000000u) return 0;
        uint32_t vend   = (seg_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        for (uint32_t va = vstart; va < vend; va += PAGE_SIZE) {
            uint32_t frame = (uint32_t)pmm_alloc_frame();
            if (!frame) {
                destroy_user_pd(pd_phys);
                return 0;
            }
            memset((void *)VIRT(frame), 0, PAGE_SIZE);
            if (vmm_map_page_in(pd_phys, va, frame, flags) < 0) {
                pmm_free_frame((void *)(uintptr_t)frame);
                destroy_user_pd(pd_phys);
                return 0;
            }
        }

        /* Copy file data a page at a time into the mapped pages. */
        for (uint32_t off = 0; off < phdr->p_filesz; ) {
            uint32_t vaddr = phdr->p_vaddr + off;
            uint32_t chunk = PAGE_SIZE - (vaddr & (PAGE_SIZE - 1));
            uint32_t remaining = phdr->p_filesz - off;
            if (chunk > remaining) chunk = remaining;

            uint8_t *dst = user_kaddr(pd_phys, vaddr);
            if (!dst) {
                destroy_user_pd(pd_phys);
                return 0;
            }
            memcpy(dst, bin + phdr->p_offset + off, chunk);
            off += chunk;
        }

        /* Track the highest byte consumed by this segment. */
        if (seg_end > top_end) top_end = seg_end;
    }

    *pd_out  = pd_phys;
    /* Round seg_end up to the next page boundary — this is where sbrk starts. */
    *brk_out = (top_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    return ehdr->e_entry;
}

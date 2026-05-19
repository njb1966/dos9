#include <vmm.h>
#include <pmm.h>
#include <kernel.h>
#include <terminal.h>
#include <stdint.h>

#define PAGE_SIZE   4096u
#define PD_ENTRIES  1024u
#define PT_ENTRIES  1024u

/*
 * Build a proper 4KB page directory and switch to it.
 *
 * Maps all physical RAM linearly at KERNEL_VIRT_BASE (0xC0000000).
 * Physical address P is accessible at virtual KERNEL_VIRT_BASE + P.
 *
 * The bootstrap 4MB PSE mapping (entries 0 + 768 of boot_page_dir) is
 * still active while we build the new structures — every pmm_alloc_frame()
 * result fits in the first 4MB, so VIRT(frame) is reachable during init.
 *
 * After loading CR3:
 *   - Entry 0 is absent  → identity map removed; kernel is higher-half only.
 *   - Entries 768..      → cover all physical RAM with 4KB pages.
 */
void vmm_init(void) {
    /* Page directory — one frame, zeroed. */
    uint32_t pd_phys = (uint32_t)pmm_alloc_frame();
    uint32_t *pd = (uint32_t *)VIRT(pd_phys);
    for (uint32_t i = 0; i < PD_ENTRIES; i++)
        pd[i] = 0;

    /* One page table per 4MB window of physical RAM.
       PD entry (768 + k) covers VMA [0xC0000000 + k*4MB, ...).
       PT entry j covers the physical frame (k*1024 + j). */
    uint32_t total    = pmm_total_frames();
    uint32_t pt_count = (total + PT_ENTRIES - 1) / PT_ENTRIES;

    for (uint32_t k = 0; k < pt_count; k++) {
        uint32_t pt_phys = (uint32_t)pmm_alloc_frame();
        uint32_t *pt = (uint32_t *)VIRT(pt_phys);

        for (uint32_t j = 0; j < PT_ENTRIES; j++) {
            uint32_t frame = k * PT_ENTRIES + j;
            pt[j] = (frame < total)
                ? (frame * PAGE_SIZE) | PF_PRESENT | PF_WRITE
                : 0;
        }
        pd[768 + k] = pt_phys | PF_PRESENT | PF_WRITE;
    }

    /* Load CR3 with the new page directory's physical address.
       Writing CR3 flushes the entire TLB. Execution continues at
       0xC010xxxx which is now backed by our new 4KB page tables. */
    __asm__ volatile("mov %0, %%cr3" :: "r"(pd_phys) : "memory");

    terminal_write("[VMM] 4KB paging active\n");
}

/*
 * Map a single 4KB virtual page to a physical frame.
 *
 * Reads CR3 to find the current page directory. All physical frames are
 * accessible at VIRT(phys) via the linear map set up by vmm_init(), so
 * we can walk and modify page tables without a separate kernel mapping.
 * Creates a new page table if the PD entry is absent.
 */
void vmm_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t pd_phys;
    __asm__ volatile("mov %%cr3, %0" : "=r"(pd_phys));
    uint32_t *pd = (uint32_t *)VIRT(pd_phys);

    uint32_t pd_idx = vaddr >> 22;
    uint32_t pt_idx = (vaddr >> 12) & 0x3FFu;

    if (!(pd[pd_idx] & PF_PRESENT)) {
        uint32_t pt_phys = (uint32_t)pmm_alloc_frame();
        uint32_t *pt = (uint32_t *)VIRT(pt_phys);
        for (uint32_t i = 0; i < PT_ENTRIES; i++) pt[i] = 0;
        pd[pd_idx] = pt_phys | PF_PRESENT | PF_WRITE;
    }

    uint32_t pt_phys = pd[pd_idx] & ~0xFFFu;
    uint32_t *pt = (uint32_t *)VIRT(pt_phys);
    pt[pt_idx] = (paddr & ~0xFFFu) | flags;

    __asm__ volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}

#include <vmm.h>
#include <pmm.h>
#include <kernel.h>
#include <terminal.h>
#include <stdint.h>

#define PAGE_SIZE   4096u
#define PD_ENTRIES  1024u
#define PT_ENTRIES  1024u

/* Page directory/table entry flags */
#define PF_PRESENT  (1u << 0)
#define PF_WRITE    (1u << 1)

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

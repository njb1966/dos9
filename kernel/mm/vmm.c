#include <vmm.h>
#include <pmm.h>
#include <kernel.h>
#include <terminal.h>
#include <stdint.h>

#define PAGE_SIZE   4096u
#define PD_ENTRIES  1024u
#define PT_ENTRIES  1024u
#define KERNEL_PD_IDX 768u   /* first PD entry covering 0xC0000000 */

static uint32_t kernel_pd_phys = 0;

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
    uint32_t max_pt   = PD_ENTRIES - KERNEL_PD_IDX;
    if (pt_count > max_pt) {
        pt_count = max_pt;
        total = max_pt * PT_ENTRIES;
        terminal_write("[VMM] RAM capped at 1GB linear map\n");
    }

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
    kernel_pd_phys = pd_phys;
    __asm__ volatile("mov %0, %%cr3" :: "r"(pd_phys) : "memory");

    terminal_write("[VMM] 4KB paging active\n");
}

uint32_t vmm_get_kernel_pd(void) { return kernel_pd_phys; }

int vmm_map_page_in(uint32_t pd_phys, uint32_t vaddr,
                    uint32_t paddr, uint32_t flags) {
    uint32_t *pd    = (uint32_t *)VIRT(pd_phys);
    uint32_t pd_idx = vaddr >> 22;
    uint32_t pt_idx = (vaddr >> 12) & 0x3FFu;

    if (!(pd[pd_idx] & PF_PRESENT)) {
        uint32_t pt_phys = (uint32_t)pmm_alloc_frame();
        if (!pt_phys) return -1;
        uint32_t *pt     = (uint32_t *)VIRT(pt_phys);
        for (uint32_t i = 0; i < PT_ENTRIES; i++) pt[i] = 0;
        /* Propagate PF_USER to the PD entry so user-mode can access the PT. */
        pd[pd_idx] = pt_phys | PF_PRESENT | PF_WRITE | (flags & PF_USER);
    }

    uint32_t pt_phys = pd[pd_idx] & ~0xFFFu;
    uint32_t *pt     = (uint32_t *)VIRT(pt_phys);
    pt[pt_idx]       = (paddr & ~0xFFFu) | flags;

    /* Only flush TLB entry if this PD is currently loaded. */
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    if (cr3 == pd_phys)
        __asm__ volatile("invlpg (%0)" :: "r"(vaddr) : "memory");

    return 0;
}

uint32_t vmm_create_user_pd(void) {
    uint32_t pd_phys   = (uint32_t)pmm_alloc_frame();
    if (!pd_phys) return 0;
    uint32_t *pd       = (uint32_t *)VIRT(pd_phys);
    uint32_t *kpd      = (uint32_t *)VIRT(kernel_pd_phys);

    /* User half: empty. */
    for (uint32_t i = 0; i < KERNEL_PD_IDX; i++) pd[i] = 0;
    /* Kernel half: share the kernel page tables (read-only from user mode). */
    for (uint32_t i = KERNEL_PD_IDX; i < PD_ENTRIES; i++) pd[i] = kpd[i];

    return pd_phys;
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
    (void)vmm_map_page_in(pd_phys, vaddr, paddr, flags);
}

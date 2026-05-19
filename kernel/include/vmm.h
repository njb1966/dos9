#pragma once
#include <stdint.h>

/* Page directory/table entry flags */
#define PF_PRESENT  (1u << 0)
#define PF_WRITE    (1u << 1)
#define PF_USER     (1u << 2)

void vmm_init(void);

/* Map vaddr → paddr in the currently-loaded page directory. */
void vmm_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);

/* Map vaddr → paddr in an explicit page directory (by physical address).
   Allocates a new page table if the PD entry is absent. */
void vmm_map_page_in(uint32_t pd_phys, uint32_t vaddr,
                     uint32_t paddr, uint32_t flags);

/* Allocate a new page directory for a user process.  Zeroes user entries
   (0-767) and copies the current kernel entries (768-1023) so the kernel
   is reachable from user-space page directories during syscalls. */
uint32_t vmm_create_user_pd(void);

/* Physical address of the kernel page directory (set by vmm_init). */
uint32_t vmm_get_kernel_pd(void);

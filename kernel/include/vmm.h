#pragma once
#include <stdint.h>

/* Page directory/table entry flags */
#define PF_PRESENT  (1u << 0)
#define PF_WRITE    (1u << 1)
#define PF_USER     (1u << 2)

void vmm_init(void);
void vmm_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);

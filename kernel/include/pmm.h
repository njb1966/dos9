#pragma once
#include <stdint.h>
#include <multiboot.h>

void     pmm_init(uint32_t magic, void *mbi_ptr);
void    *pmm_alloc_frame(void);
void     pmm_free_frame(void *addr);
uint32_t pmm_free_frames(void);
uint32_t pmm_total_frames(void);

/* Snapshot of multiboot module descriptors taken at pmm_init time.
   Captured BEFORE the bitmap is written, so the data survives even when
   the multiboot mods array sits at _kernel_end (it usually does). */
uint32_t              pmm_mod_count(void);
struct multiboot_mod *pmm_mod(uint32_t i);   /* NULL if out of range */

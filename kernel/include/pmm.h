#pragma once
#include <stdint.h>

void     pmm_init(uint32_t magic, void *mbi_ptr);
void    *pmm_alloc_frame(void);
void     pmm_free_frame(void *addr);
uint32_t pmm_free_frames(void);
uint32_t pmm_total_frames(void);

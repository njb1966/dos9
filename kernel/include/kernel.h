#pragma once
#include <stdint.h>

/*
 * The kernel is linked at virtual 0xC0000000 + 1MB and loaded at physical
 * 1MB.  These macros convert between the two address spaces.
 */
#define KERNEL_VIRT_BASE 0xC0000000U

#define PHYS(vaddr)  ((uint32_t)(vaddr) - KERNEL_VIRT_BASE)
#define VIRT(paddr)  ((void *)((uint32_t)(paddr) + KERNEL_VIRT_BASE))

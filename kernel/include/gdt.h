#pragma once
#include <stdint.h>

/* Segment selectors — (index << 3) | RPL */
#define SEG_KERNEL_CS   0x08   /* ring 0 code */
#define SEG_KERNEL_DS   0x10   /* ring 0 data */
#define SEG_USER_CS     0x18   /* ring 3 code (without RPL bits) */
#define SEG_USER_DS     0x20   /* ring 3 data (without RPL bits) */
#define SEG_USER_CS_R3  0x1B   /* ring 3 code — load into CS for ring-3 iret */
#define SEG_USER_DS_R3  0x23   /* ring 3 data — load into DS/ES/SS for ring-3 iret */
#define SEG_TSS         0x28   /* task state segment */

void gdt_init(void);

/* Update TSS.esp0 — call on every context switch to a new process so
   that int 0x80 / hardware interrupts from ring 3 switch to the right
   kernel stack. */
void gdt_set_kernel_stack(uint32_t esp0);

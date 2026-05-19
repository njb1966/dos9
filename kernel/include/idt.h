#pragma once
#include <stdint.h>

/* CPU register state at interrupt — laid out to match isr_common stack frame */
struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* pusha order */
    uint32_t int_no, err_code;                         /* pushed by ISR stub */
    uint32_t eip, cs, eflags, useresp, ss;             /* pushed by CPU */
};

#define INT_GATE    0x8E   /* present, ring 0, 32-bit interrupt gate */
#define INT_GATE_U  0xEE   /* present, ring 3, 32-bit interrupt gate (user-callable) */

void idt_init(void);
void idt_set_gate(int n, void (*handler)(void), uint8_t type_attr);

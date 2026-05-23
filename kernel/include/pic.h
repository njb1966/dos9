#pragma once
#include <idt.h>

/* IRQ numbers (relative to PIC, not vector) */
#define IRQ_TIMER    0
#define IRQ_KEYBOARD 1

typedef void (*irq_handler_t)(struct registers *);

void pic_init(void);
void pic_eoi(int irq);
void irq_register(int irq, irq_handler_t handler);
void pic_unmask_irq(int irq);

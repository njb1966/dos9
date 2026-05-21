#include <pic.h>
#include <io.h>
#include <stddef.h>

/* 8259 PIC I/O ports */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

/* Initialization Command Words */
#define ICW1_INIT    0x10   /* initialization */
#define ICW1_ICW4    0x01   /* ICW4 will follow */
#define ICW4_8086    0x01   /* 8086 mode */

#define PIC_EOI      0x20   /* end-of-interrupt command */

/* IRQ vectors after remapping */
#define IRQ_BASE_MASTER 32
#define IRQ_BASE_SLAVE  40

static irq_handler_t irq_handlers[16];

void pic_init(void) {
    /* save masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* start initialization sequence (cascade mode) */
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();

    /* ICW2: vector offsets */
    outb(PIC1_DATA, IRQ_BASE_MASTER); io_wait();
    outb(PIC2_DATA, IRQ_BASE_SLAVE);  io_wait();

    /* ICW3: cascade wiring — master has slave on IRQ2, slave ID is 2 */
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* restore saved masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_eoi(int irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void irq_register(int irq, irq_handler_t handler) {
    if (irq < 16)
        irq_handlers[irq] = handler;
}

void pic_unmask_irq(int irq) {
    if (irq < 8) {
        outb(PIC1_DATA, inb(PIC1_DATA) & (uint8_t)~(1u << irq));
    } else {
        outb(PIC2_DATA, inb(PIC2_DATA) & (uint8_t)~(1u << (irq - 8)));
        outb(PIC1_DATA, inb(PIC1_DATA) & (uint8_t)~(1u << 2));  /* unmask cascade */
    }
}

/* Called from irq_common in isr.S */
void irq_handler(struct registers *r) {
    int irq = r->int_no - IRQ_BASE_MASTER;
    /*
     * Send EOI before dispatching.  If the handler does a context switch
     * and never returns on this stack (scheduler), the PIC is still
     * re-armed and future IRQs are not lost.
     */
    pic_eoi(irq);
    if (irq_handlers[irq])
        irq_handlers[irq](r);
}

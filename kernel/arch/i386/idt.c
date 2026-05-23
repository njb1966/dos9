#include <idt.h>
#include <terminal.h>
#include <stdint.h>

struct idt_entry {
    uint16_t offset_low;   /* bits 0-15 of handler address */
    uint16_t selector;     /* code segment selector */
    uint8_t  zero;         /* always 0 */
    uint8_t  type_attr;    /* gate type, DPL, present */
    uint16_t offset_high;  /* bits 16-31 of handler address */
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

/* Exception stubs (isr.S) */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

/* IRQ stubs (isr.S) — vectors 32-47 */
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

static const char *exception_names[] = {
    "Divide by Zero",         /* 0  */
    "Debug",                  /* 1  */
    "NMI",                    /* 2  */
    "Breakpoint",             /* 3  */
    "Overflow",               /* 4  */
    "Bound Range Exceeded",   /* 5  */
    "Invalid Opcode",         /* 6  */
    "Device Not Available",   /* 7  */
    "Double Fault",           /* 8  */
    "Coprocessor Seg Overrun",/* 9  */
    "Invalid TSS",            /* 10 */
    "Segment Not Present",    /* 11 */
    "Stack Fault",            /* 12 */
    "General Protection",     /* 13 */
    "Page Fault",             /* 14 */
    "Reserved",               /* 15 */
    "x87 FP Exception",       /* 16 */
    "Alignment Check",        /* 17 */
    "Machine Check",          /* 18 */
    "SIMD FP Exception",      /* 19 */
    "Virtualization",         /* 20 */
};

void idt_set_gate(int n, void (*handler)(void), uint8_t type_attr) {
    uint32_t addr = (uint32_t)handler;
    idt[n].offset_low  = addr & 0xFFFF;
    idt[n].selector    = 0x08;
    idt[n].zero        = 0;
    idt[n].type_attr   = type_attr;
    idt[n].offset_high = (addr >> 16) & 0xFFFF;
}

static void idt_set_entry(int i, void (*handler)(void)) {
    idt_set_gate(i, handler, INT_GATE);
}

/* Called from isr_common in isr.S */
void exception_handler(struct registers *r) {
    terminal_write("\n*** EXCEPTION ");
    if (r->int_no < 21)
        terminal_write(exception_names[r->int_no]);
    else
        terminal_write("Reserved");
    terminal_write(" (#");
    terminal_writehex(r->int_no);
    terminal_write(") ***\n");
    terminal_write("  EIP="); terminal_writehex(r->eip);
    terminal_write("  ERR="); terminal_writehex(r->err_code);
    terminal_write("  CS=");  terminal_writehex(r->cs);
    terminal_write("\n");

    __asm__ volatile("cli; hlt");
    __builtin_unreachable();
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    /* zero the whole table — absent entries have present=0 and are ignored */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low  = 0;
        idt[i].selector    = 0;
        idt[i].zero        = 0;
        idt[i].type_attr   = 0;
        idt[i].offset_high = 0;
    }

    idt_set_entry(0,  isr0);  idt_set_entry(1,  isr1);
    idt_set_entry(2,  isr2);  idt_set_entry(3,  isr3);
    idt_set_entry(4,  isr4);  idt_set_entry(5,  isr5);
    idt_set_entry(6,  isr6);  idt_set_entry(7,  isr7);
    idt_set_entry(8,  isr8);  idt_set_entry(9,  isr9);
    idt_set_entry(10, isr10); idt_set_entry(11, isr11);
    idt_set_entry(12, isr12); idt_set_entry(13, isr13);
    idt_set_entry(14, isr14); idt_set_entry(15, isr15);
    idt_set_entry(16, isr16); idt_set_entry(17, isr17);
    idt_set_entry(18, isr18); idt_set_entry(19, isr19);
    idt_set_entry(20, isr20); idt_set_entry(21, isr21);
    idt_set_entry(22, isr22); idt_set_entry(23, isr23);
    idt_set_entry(24, isr24); idt_set_entry(25, isr25);
    idt_set_entry(26, isr26); idt_set_entry(27, isr27);
    idt_set_entry(28, isr28); idt_set_entry(29, isr29);
    idt_set_entry(30, isr30); idt_set_entry(31, isr31);

    /* IRQ vectors 32-47 */
    idt_set_entry(32, irq0);  idt_set_entry(33, irq1);
    idt_set_entry(34, irq2);  idt_set_entry(35, irq3);
    idt_set_entry(36, irq4);  idt_set_entry(37, irq5);
    idt_set_entry(38, irq6);  idt_set_entry(39, irq7);
    idt_set_entry(40, irq8);  idt_set_entry(41, irq9);
    idt_set_entry(42, irq10); idt_set_entry(43, irq11);
    idt_set_entry(44, irq12); idt_set_entry(45, irq13);
    idt_set_entry(46, irq14); idt_set_entry(47, irq15);

    __asm__ volatile("lidt (%0)" : : "r"(&idtp));
    __asm__ volatile("sti");
}

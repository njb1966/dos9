#include <terminal.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <keyboard.h>
#include <shell.h>
#include <pmm.h>
#include <vmm.h>
#include <kheap.h>
#include <stdint.h>

static volatile uint32_t timer_ticks = 0;

static void timer_handler(struct registers *r) {
    (void)r;
    timer_ticks++;
}

void kernel_main(uint32_t mb_magic, void *mb_info) {
    terminal_init();
    terminal_write("DOS/9\n");
    terminal_write("-----\n");

    gdt_init();
    terminal_write("[GDT] loaded\n");

    idt_init();
    terminal_write("[IDT] loaded\n");

    pic_init();
    irq_register(IRQ_TIMER, timer_handler);
    terminal_write("[PIC] remapped, IRQs active\n");

    keyboard_init();
    terminal_write("[KBD] ready\n");

    pmm_init(mb_magic, mb_info);
    vmm_init();
    kheap_init();

    shell_run();
}

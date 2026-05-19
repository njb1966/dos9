#include <terminal.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <keyboard.h>
#include <shell.h>
#include <pmm.h>
#include <vmm.h>
#include <kheap.h>
#include <pit.h>
#include <process.h>
#include <devfs.h>
#include <procfs.h>
#include <modfs.h>
#include <vfs.h>
#include <syscall.h>
#include <ata.h>
#include <diskfs.h>
#include <kernel.h>
#include <stdint.h>

/* Spins a character in the top-right corner of the VGA screen.
   Demonstrates preemptive scheduling — this runs concurrently with the shell. */
static void spinner_task(void) {
    volatile uint16_t *vga = (volatile uint16_t *)0xC00B8000;
    const char spin[] = "|/-\\";
    for (uint32_t i = 0;;i++) {
        vga[79] = (uint16_t)(0x0F00 | (unsigned char)spin[i & 3]);
        for (volatile uint32_t d = 0; d < 2000000; d++);
    }
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
    terminal_write("[PIC] remapped, IRQs active\n");

    keyboard_init();
    terminal_write("[KBD] ready\n");

    pmm_init(mb_magic, mb_info);
    vmm_init();
    kheap_init();

    pit_init(100);
    terminal_write("[PIT] 100 Hz\n");

    vfs_init();
    devfs_init();
    procfs_init();
    modfs_init();
    vfs_open_stdio();
    terminal_write("[VFS] / /dev /proc /mod mounted\n");

    syscall_init();
    terminal_write("[SYSCALL] int 0x80 gate active\n");

    ata_init();
    if (ata_present()) diskfs_init();

    process_init();
    process_create(spinner_task, "spinner");
    terminal_write("[PROC] scheduler active\n");

    (void)mb_magic; (void)mb_info;   /* now consumed by pmm_init / modfs */

    shell_run();
}

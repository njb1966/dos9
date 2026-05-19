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
#include <vfs.h>
#include <syscall.h>
#include <elf.h>
#include <multiboot.h>
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

    devfs_init();
    procfs_init();
    vfs_init();
    terminal_write("[VFS] mounted\n");

    syscall_init();
    terminal_write("[SYSCALL] int 0x80 gate active\n");

    process_init();
    process_create(spinner_task, "spinner");
    terminal_write("[PROC] scheduler active\n");

    /* If a Multiboot module was passed (via -initrd), load it as a user ELF.
       mb_info is a physical address; use VIRT() now that the identity map
       has been replaced by the kernel linear map set up by vmm_init(). */
    struct multiboot_info *mbi =
        (struct multiboot_info *)VIRT((uint32_t)mb_info);
    if (mb_magic == MULTIBOOT_MAGIC &&
        (mbi->flags & MULTIBOOT_FLAG_MODS) && mbi->mods_count > 0) {
        struct multiboot_mod *mod =
            (struct multiboot_mod *)VIRT(mbi->mods_addr);
        const void *elf_data = (const void *)VIRT(mod->mod_start);
        uint32_t    elf_size = mod->mod_end - mod->mod_start;
        uint32_t    pd_phys  = 0;
        uint32_t    entry    = elf_load(elf_data, elf_size, &pd_phys);
        if (entry) {
            process_create_user(entry, "init", pd_phys);
            terminal_write("[ELF] user process loaded\n");
        } else {
            terminal_write("[ELF] load failed\n");
        }
    }

    shell_run();
}

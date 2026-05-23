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
#include <rtc.h>
#include <process.h>
#include <devfs.h>
#include <procfs.h>
#include <modfs.h>
#include <vfs.h>
#include <syscall.h>
#include <ata.h>
#include <diskfs.h>
#include <net_init.h>
#include <dhcp.h>
#include <netif.h>
#include <net.h>
#include <multiboot.h>
#include <string.h>
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

static int cmdline_has_netmode_bridge(void *mb_info) {
    if (!mb_info) return 0;
    const struct multiboot_info *mi = (const struct multiboot_info *)mb_info;
    if (!mi->cmdline) return 0;

    const char *cmd = (const char *)VIRT(mi->cmdline);
    const char *needle = "netmode=bridge";
    for (const char *p = cmd; *p; p++) {
        const char *a = p;
        const char *b = needle;
        while (*a && *b && *a == *b) {
            a++;
            b++;
        }
        if (*b == '\0' &&
            (p == cmd || p[-1] == ' ') &&
            (*a == '\0' || *a == ' '))
            return 1;
    }
    return 0;
}

void kernel_main(uint32_t mb_magic, void *mb_info) {
    int bridge_mode = cmdline_has_netmode_bridge(mb_info);

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
    rtc_init();
    terminal_write("[RTC] CMOS snapshot captured\n");

    vfs_init();
    devfs_init();
    procfs_init();
    modfs_init();

    process_init();         /* must be before vfs_open_stdio — fds live in process_t */
    vfs_open_stdio();
    terminal_write("[VFS] / /dev /proc /mod mounted\n");

    syscall_init();
    terminal_write("[SYSCALL] int 0x80 gate active\n");

    net_init();
    /* Wait up to ~3 s for DHCP, then retry once (NIC may not be ready
       when the first DISCOVER is sent during early boot). */
    for (int i = 0; i < 300 && !dhcp_done(); i++)
        schedule();
    if (!dhcp_done()) {
        dhcp_start();   /* retry DISCOVER */
        for (int i = 0; i < 500 && !dhcp_done(); i++)
            schedule();
    }
    if (!dhcp_done()) {
        if (!bridge_mode) {
            /* Dev-only fallback for QEMU user-mode networking.
               This keeps the inner loop usable if SLIRP's DHCP offer is
               missed at boot, but it is not a deployment contract. */
            g_netif.ip      = IP4(10,0,2,15);
            g_netif.netmask = IP4(255,255,255,0);
            g_netif.gateway = IP4(10,0,2,2);
            /* SLIRP exposes its DNS forwarder at 10.0.2.3. */
            g_netif.dns     = IP4(10,0,2,3);
            g_netif.up      = 1;
            terminal_write("[DHCP] timeout -- QEMU user-net fallback 10.0.2.15\n");
        } else {
            terminal_write("[DHCP] timeout -- bridge mode, no SLIRP fallback\n");
        }
    }

    ata_init();
    uint32_t sh_pid = 0;
    if (ata_present()) {
        diskfs_init();
        sh_pid = shell_exec_user("/disk/sh");
        if (sh_pid)
            terminal_write("[INIT] /disk/sh launched\n");
    }

    process_create(spinner_task, "spinner");
    terminal_write("[PROC] scheduler active\n");

    (void)mb_magic;   /* now consumed by pmm_init / modfs */

    /* Wait for the user shell to exit before touching the keyboard. */
    if (sh_pid) {
        for (;;) {
            int alive = 0;
            for (int i = 0, n = process_count(); i < n; i++) {
                process_t *p = process_get(i);
                if (!p) continue;
                if (p->pid == sh_pid && p->state != PROC_DEAD) { alive = 1; break; }
            }
            if (!alive) break;
            schedule();
        }
        terminal_write("\n[INIT] sh exited -- kernel shell\n");
    }

    shell_run();
}

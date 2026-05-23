/* Multiboot header constants */
.set ALIGN,          1 << 0
.set MEMINFO,        1 << 1
.set FLAGS,          ALIGN | MEMINFO
.set MAGIC,          0x1BADB002
.set CHECKSUM,       -(MAGIC + FLAGS)
.set KERNEL_VIRT_BASE, 0xC0000000

/* Multiboot header — must be in first 8KB of kernel image */
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* Initial kernel stack — virtual address, used after paging is live */
.section .bss
.align 16
stack_bottom:
.skip 16384     /* 16KB */
stack_top:

/*
 * Bootstrap page directory — 4KB-aligned, stored in .data so GRUB loads it
 * to its physical address (LMA).
 *
 * Uses 4MB pages (PSE) so we need no page tables, just this one directory.
 *
 * Entry   0: identity-map 0x00000000–0x003FFFFF → phys 0x00000000.
 *   Required: right after CR0.PG is set the CPU is still at a physical
 *   address (~0x100000); the identity map keeps it valid.
 *
 * Entry 768: map 0xC0000000–0xC03FFFFF → phys 0x00000000.
 *   The kernel's higher-half virtual range.
 *
 * 4MB page directory entry encoding:
 *   bits[31:22] = physical base addr >> 22
 *   bit 7 (PS)  = 1  (4MB page, requires CR4.PSE)
 *   bit 1 (R/W) = 1
 *   bit 0 (P)   = 1
 */
.section .data
.align 4096
boot_page_dir:
    .long 0x00000083        /* entry   0: identity-map first 4MB */
    .fill 767, 4, 0         /* entries 1–767: not present */
    .long 0x00000083        /* entry 768: kernel higher-half mapping */
    .fill 255, 4, 0         /* entries 769–1023: not present */

/*
 * Kernel entry point.
 * GRUB leaves us in 32-bit protected mode with:
 *   %eax = Multiboot magic (0x2BADB002)
 *   %ebx = physical address of Multiboot info struct
 *
 * We must not use the stack before higher_half: stack_top is a virtual
 * address and the stack is not accessible until paging is enabled.
 */
.section .text
.global _start
.type _start, @function
_start:
    /* 1. Enable 4MB pages (PSE) in CR4. */
    mov %cr4, %ecx
    or  $0x10, %ecx
    mov %ecx, %cr4

    /* 2. Load CR3 with the physical address of the boot page directory.
          boot_page_dir is a virtual symbol; subtract KERNEL_VIRT_BASE to
          get the LMA (physical address GRUB loaded it to). */
    mov $(boot_page_dir - KERNEL_VIRT_BASE), %ecx
    mov %ecx, %cr3

    /* 3. Enable paging (set CR0.PG). */
    mov %cr0, %ecx
    or  $0x80000000, %ecx
    mov %ecx, %cr0

    /*
     * 4. Long jump to the higher half.
     *    The CPU is still executing at identity-mapped physical addresses.
     *    Loading %ecx with the VMA of higher_half and jumping there
     *    switches us to the kernel's virtual address space.
     */
    lea higher_half, %ecx
    jmp *%ecx

higher_half:
    /* 5. Set up the kernel stack (virtual addr now valid via paging). */
    mov $stack_top, %esp

    /*
     * 6. Pass Multiboot args to kernel_main(uint32_t magic, void *mbi).
     *    %ebx is a physical address in low memory; it remains accessible
     *    via the identity map (physical < 4MB).
     *    cdecl: push right-to-left, so push mbi first, then magic.
     */
    push %ebx
    push %eax
    call kernel_main

    cli
1:  hlt
    jmp 1b
.size _start, . - _start

ENTRY(_start)

KERNEL_VIRT_BASE = 0xC0000000;

SECTIONS
{
    /*
     * VMA: kernel linked at 0xC0100000 (higher half, 3GB + 1MB).
     * LMA: each section's AT() sets the physical load address.
     *      LMA = VMA - KERNEL_VIRT_BASE  (e.g. 0xC0100000 → 0x100000).
     * GRUB loads segments to their LMA; paging then maps them at VMA.
     */
    . = 0xC0100000;

    .text : AT(ADDR(.text) - KERNEL_VIRT_BASE) {
        *(.multiboot)
        *(.text)
    }

    . = ALIGN(4K);
    .rodata : AT(ADDR(.rodata) - KERNEL_VIRT_BASE) {
        *(.rodata)
    }

    . = ALIGN(4K);
    .data : AT(ADDR(.data) - KERNEL_VIRT_BASE) {
        *(.data)
    }

    . = ALIGN(4K);
    .bss : AT(ADDR(.bss) - KERNEL_VIRT_BASE) {
        _bss_start = .;
        *(COMMON)
        *(.bss)
    }

    . = ALIGN(4K);
    _kernel_end = .;    /* virtual; PHYS(_kernel_end) gives physical end */
}

/*
 * gdt_flush(uint32_t gdtp_addr)
 *
 * Load the GDT from the gdtp struct at gdtp_addr, then reload all segment
 * registers. The code segment can only be reloaded via a far jump.
 */
.global gdt_flush
.type gdt_flush, @function
gdt_flush:
    mov  4(%esp), %eax      /* gdtp_addr argument */
    lgdt (%eax)             /* load the GDT */

    /* Far jump to reload CS with the kernel code selector (0x08).
     * This is the only way to update CS — mov doesn't work on it. */
    ljmp $0x08, $.reload_cs
.reload_cs:
    /* Reload all data segment registers with kernel data selector (0x10) */
    mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs
    mov  %ax, %ss
    ret
.size gdt_flush, . - gdt_flush

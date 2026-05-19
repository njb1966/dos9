/* Multiboot header constants */
.set ALIGN,    1 << 0
.set MEMINFO,  1 << 1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

/* Multiboot header — must be in first 8KB of kernel */
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* Reserve initial kernel stack */
.section .bss
.align 16
stack_bottom:
.skip 16384     /* 16KB */
stack_top:

/* Kernel entry point */
.section .text
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp    /* set up stack */
    push %ebx               /* arg2: Multiboot info pointer */
    push %eax               /* arg1: Multiboot magic */
    call kernel_main         /* jump to C */
    cli                      /* disable interrupts if we return */
1:  hlt                      /* halt */
    jmp 1b
.size _start, . - _start

/*
 * ISR stubs for CPU exceptions 0-31.
 *
 * Each stub pushes a consistent stack frame and jumps to isr_common:
 *   [err_code]  — CPU-pushed (real) or 0 (dummy for exceptions without one)
 *   [int_no]    — we push this
 *
 * isr_common then saves all registers, switches to kernel DS, calls the
 * C exception_handler, restores, and returns via iret.
 */

.macro ISR_NOERRCODE num
.global isr\num
.type isr\num, @function
isr\num:
    pushl $0        /* dummy error code */
    pushl $\num
    jmp isr_common
.endm

.macro ISR_ERRCODE num
.global isr\num
.type isr\num, @function
isr\num:
    pushl $\num     /* CPU already pushed the real error code */
    jmp isr_common
.endm

ISR_NOERRCODE  0   /* Divide by Zero */
ISR_NOERRCODE  1   /* Debug */
ISR_NOERRCODE  2   /* NMI */
ISR_NOERRCODE  3   /* Breakpoint */
ISR_NOERRCODE  4   /* Overflow */
ISR_NOERRCODE  5   /* Bound Range Exceeded */
ISR_NOERRCODE  6   /* Invalid Opcode */
ISR_NOERRCODE  7   /* Device Not Available */
ISR_ERRCODE    8   /* Double Fault */
ISR_NOERRCODE  9   /* Coprocessor Segment Overrun (obsolete) */
ISR_ERRCODE   10   /* Invalid TSS */
ISR_ERRCODE   11   /* Segment Not Present */
ISR_ERRCODE   12   /* Stack Fault */
ISR_ERRCODE   13   /* General Protection Fault */
ISR_ERRCODE   14   /* Page Fault */
ISR_NOERRCODE 15   /* Reserved */
ISR_NOERRCODE 16   /* x87 FP Exception */
ISR_ERRCODE   17   /* Alignment Check */
ISR_NOERRCODE 18   /* Machine Check */
ISR_NOERRCODE 19   /* SIMD FP Exception */
ISR_NOERRCODE 20   /* Virtualization Exception */
ISR_NOERRCODE 21   /* Reserved */
ISR_NOERRCODE 22   /* Reserved */
ISR_NOERRCODE 23   /* Reserved */
ISR_NOERRCODE 24   /* Reserved */
ISR_NOERRCODE 25   /* Reserved */
ISR_NOERRCODE 26   /* Reserved */
ISR_NOERRCODE 27   /* Reserved */
ISR_NOERRCODE 28   /* Reserved */
ISR_NOERRCODE 29   /* Reserved */
ISR_NOERRCODE 30   /* Reserved */
ISR_NOERRCODE 31   /* Reserved */

/* Hardware IRQ stubs — vectors 32-47 (after PIC remapping) */
.macro IRQ irqnum vecnum
.global irq\irqnum
.type irq\irqnum, @function
irq\irqnum:
    pushl $0
    pushl $\vecnum
    jmp irq_common
.endm

IRQ  0, 32    /* Timer */
IRQ  1, 33    /* Keyboard */
IRQ  2, 34    /* Cascade (internal) */
IRQ  3, 35    /* COM2 */
IRQ  4, 36    /* COM1 */
IRQ  5, 37    /* LPT2 */
IRQ  6, 38    /* Floppy */
IRQ  7, 39    /* LPT1 / spurious */
IRQ  8, 40    /* RTC */
IRQ  9, 41    /* free */
IRQ 10, 42    /* free */
IRQ 11, 43    /* free */
IRQ 12, 44    /* PS/2 mouse */
IRQ 13, 45    /* FPU */
IRQ 14, 46    /* Primary ATA */
IRQ 15, 47    /* Secondary ATA / spurious */

/* Common IRQ handler — saves state, calls irq_handler, restores */
.type irq_common, @function
irq_common:
    pusha
    mov  %ds, %ax
    pushl %eax
    mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs
    pushl %esp
    call irq_handler
    addl $4, %esp
    popl %eax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs
    popa
    addl $8, %esp
    iret

/* System call gate — int 0x80 */
.global isr128
.type isr128, @function
isr128:
    pushl $0       /* dummy err_code */
    pushl $128     /* int_no = 0x80 */
    jmp syscall_common

/*
 * syscall_common — mirrors irq_common but calls syscall_handler.
 *
 * syscall_handler(struct registers *r) writes its return value into
 * r->eax, which is a slot in the pusha frame on the stack.  popa then
 * restores EAX from that slot, so the caller sees the return value.
 */
.type syscall_common, @function
syscall_common:
    pusha
    mov  %ds, %ax
    pushl %eax
    mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs
    pushl %esp
    call syscall_handler
    addl $4, %esp
    popl %eax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs
    popa
    addl $8, %esp
    iret

.type isr_common, @function
isr_common:
    pusha                   /* push eax,ecx,edx,ebx,esp,ebp,esi,edi */

    mov  %ds, %ax
    pushl %eax              /* save caller's data segment */

    mov  $0x10, %ax         /* switch to kernel data segment */
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs

    pushl %esp              /* pass pointer to struct registers */
    call exception_handler
    addl $4, %esp

    popl %eax               /* restore caller's data segment */
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs

    popa
    addl $8, %esp           /* discard int_no and err_code */
    iret

/*
 * void context_switch(process_t *prev, process_t *next)
 *
 * Saves callee-saved registers + ESP to prev, restores from next.
 * EIP is implicitly saved/restored as the return address on the stack.
 *
 * Stack layout after the four pushes below:
 *   [esp+ 0] EBX  (last pushed)
 *   [esp+ 4] ESI
 *   [esp+ 8] EDI
 *   [esp+12] EBP  (first pushed)
 *   [esp+16] return address
 *   [esp+20] prev  (first arg, cdecl)
 *   [esp+24] next  (second arg)
 *
 * process_t field offsets (must match process.h):
 *   0: esp
 *   4: page_dir  (physical CR3 value)
 */

.global context_switch
.type   context_switch, @function

context_switch:
    push %ebp
    push %edi
    push %esi
    push %ebx

    mov  20(%esp), %eax         /* prev */
    mov  %esp, 0(%eax)          /* prev->esp = esp */

    mov  24(%esp), %ecx         /* next (loaded before esp changes) */
    mov  0(%ecx), %esp          /* esp = next->esp */

    /* Switch page directory only if it differs (avoids unnecessary TLB flush). */
    mov  4(%ecx), %edx          /* next->page_dir */
    mov  %cr3, %eax
    cmp  %eax, %edx
    je   .Lskip_cr3
    mov  %edx, %cr3
.Lskip_cr3:

    pop  %ebx
    pop  %esi
    pop  %edi
    pop  %ebp
    ret

.size context_switch, . - context_switch

/*
 * void enter_userspace(uint32_t user_eip, uint32_t user_esp)
 *
 * Switches CPU from ring 0 to ring 3 via iret.  Never returns.
 * Called from user_trampoline() after segment registers and TSS are set.
 *
 * Stack on entry (cdecl):
 *   [esp+4] = user_eip
 *   [esp+8] = user_esp
 */
.global enter_userspace
.type   enter_userspace, @function
enter_userspace:
    movl  8(%esp), %ecx    /* user_esp */
    movl  4(%esp), %edx    /* user_eip */

    movw  $0x23, %ax       /* user data selector: index 4 | RPL=3 */
    movw  %ax, %ds
    movw  %ax, %es
    movw  %ax, %fs
    movw  %ax, %gs

    pushl $0x23            /* SS  = user data | RPL=3 */
    pushl %ecx             /* ESP = user stack */
    pushfl
    orl   $0x200, (%esp)   /* ensure IF=1 in saved EFLAGS */
    pushl $0x1B            /* CS  = user code | RPL=3 */
    pushl %edx             /* EIP = user entry point */
    iret
.size enter_userspace, . - enter_userspace

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

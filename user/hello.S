/* Minimal user-space hello world for DOS/9.
   Linked at 0x400000.  Uses int 0x80 syscall ABI:
     eax = syscall number
     ebx = arg1, ecx = arg2, edx = arg3 */

.section .text
.global _start
_start:
    /* SYS_WRITE(fd=1, buf=msg, len=23) */
    movl $2,     %eax
    movl $1,     %ebx
    movl $msg,   %ecx
    movl $23,    %edx
    int  $0x80

    /* SYS_EXIT(code=0) */
    movl $0, %eax
    movl $0, %ebx
    int  $0x80

    /* Should never reach here */
    hlt

.section .rodata
msg:
    .ascii "Hello from user space!\n"

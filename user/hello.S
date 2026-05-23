/* Minimal user-space hello for DOS/9.
   Writes a greeting via int 0x80 SYS_WRITE, then loops forever so the
   process is still alive when the shell tries `rm /proc/<pid>`.

   Linked at 0x400000.  Syscall ABI: eax = number, ebx/ecx/edx = args. */

.section .text
.global _start
_start:
    /* SYS_WRITE(fd=1, buf=msg, len=23) */
    movl $2,     %eax
    movl $1,     %ebx
    movl $msg,   %ecx
    movl $23,    %edx
    int  $0x80

    /* Spin at ring 3 until the kernel marks us dead.  hlt is privileged
       and would #GP, so we just yield-by-pause and loop. */
1:  pause
    jmp 1b

.section .rodata
msg:
    .ascii "Hello from user space!\n"

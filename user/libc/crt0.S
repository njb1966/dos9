/* crt0.S — C runtime startup for DOS/9 user-space programs.
   The kernel sets ESP = USER_STACK_TOP (0xBFFF0000, 16-byte aligned).
   We align the stack, push argc=0/argv=NULL, then call main.
   On return, pass main's exit code to exit(). */

.section .text
.global _start
_start:
    andl  $-16, %esp        /* guarantee 16-byte alignment */
    subl  $8,   %esp        /* pad so (argc + argv + pad) = 16 bytes total */
    pushl $0                /* argv = NULL */
    pushl $0                /* argc = 0 */
    call  main
    addl  $16,  %esp        /* discard args + pad */
    pushl %eax              /* exit code = main() return value */
    call  exit
1:  jmp   1b               /* unreachable */

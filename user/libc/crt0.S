/* crt0.S — C runtime startup for DOS/9 user-space programs.
   The kernel sets up the user stack with:
     [esp+0]  = argc
     [esp+4]  = argv[0]   (pointer to first argument string)
     ...
     [esp+4*(argc+1)] = NULL sentinel
   We save argc and compute argv before aligning the stack, then call main. */

.section .text
.global _start
_start:
    movl  (%esp),  %ebx     /* ebx = argc */
    leal  4(%esp), %edx     /* edx = argv (pointer to argv[0]) */
    andl  $-16, %esp        /* 16-byte align */
    subl  $8,   %esp        /* two padding words */
    pushl %edx              /* argv */
    pushl %ebx              /* argc */
    call  main
    addl  $16,  %esp
    pushl %eax
    call  exit
1:  jmp   1b

1000         # qemu -kernel loads the kernel at 0x80000000
1001         # and causes each hart (i.e. CPU) to jump there.
1002         # kernel.ld causes the following code to
1003         # be placed at 0x80000000.
1004 .section .text
1005 .global _entry
1006 _entry:
1007         # set up a stack for C.
1008         # stack0 is declared in start.c,
1009         # with a 4096-byte stack per CPU.
1010         # sp = stack0 + ((hartid + 1) * 4096)
1011         la sp, stack0
1012         li a0, 1024*4
1013         csrr a1, mhartid
1014         addi a1, a1, 1
1015         mul a0, a0, a1
1016         add sp, sp, a0
1017         # jump to start() in start.c
1018         call start
1019 spin:
1020         j spin
1021 
1022 
1023 
1024 
1025 
1026 
1027 
1028 
1029 
1030 
1031 
1032 
1033 
1034 
1035 
1036 
1037 
1038 
1039 
1040 
1041 
1042 
1043 
1044 
1045 
1046 
1047 
1048 
1049 

6900 #include "types.h"
6901 #include "param.h"
6902 #include "memlayout.h"
6903 #include "riscv.h"
6904 #include "defs.h"
6905 
6906 //
6907 // the riscv Platform Level Interrupt Controller (PLIC).
6908 //
6909 
6910 void
6911 plicinit(void)
6912 {
6913   // set desired IRQ priorities non-zero (otherwise disabled).
6914   *(uint32*)(PLIC + UART0_IRQ*4) = 1;
6915   *(uint32*)(PLIC + VIRTIO0_IRQ*4) = 1;
6916 }
6917 
6918 void
6919 plicinithart(void)
6920 {
6921   int hart = cpuid();
6922 
6923   // set enable bits for this hart's S-mode
6924   // for the uart and virtio disk.
6925   *(uint32*)PLIC_SENABLE(hart) = (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);
6926 
6927   // set this hart's S-mode priority threshold to 0.
6928   *(uint32*)PLIC_SPRIORITY(hart) = 0;
6929 }
6930 
6931 // ask the PLIC what interrupt we should serve.
6932 int
6933 plic_claim(void)
6934 {
6935   int hart = cpuid();
6936   int irq = *(uint32*)PLIC_SCLAIM(hart);
6937   return irq;
6938 }
6939 
6940 // tell the PLIC we've served this IRQ.
6941 void
6942 plic_complete(int irq)
6943 {
6944   int hart = cpuid();
6945   *(uint32*)PLIC_SCLAIM(hart) = irq;
6946 }
6947 
6948 
6949 

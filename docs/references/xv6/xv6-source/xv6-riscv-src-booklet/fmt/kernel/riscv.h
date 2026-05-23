0500 #ifndef __ASSEMBLER__
0501 
0502 // which hart (core) is this?
0503 static inline uint64
0504 r_mhartid()
0505 {
0506   uint64 x;
0507   asm volatile("csrr %0, mhartid" : "=r" (x) );
0508   return x;
0509 }
0510 
0511 // Machine Status Register, mstatus
0512 
0513 #define MSTATUS_MPP_MASK (3L << 11) // previous mode.
0514 #define MSTATUS_MPP_M (3L << 11)
0515 #define MSTATUS_MPP_S (1L << 11)
0516 #define MSTATUS_MPP_U (0L << 11)
0517 
0518 static inline uint64
0519 r_mstatus()
0520 {
0521   uint64 x;
0522   asm volatile("csrr %0, mstatus" : "=r" (x) );
0523   return x;
0524 }
0525 
0526 static inline void
0527 w_mstatus(uint64 x)
0528 {
0529   asm volatile("csrw mstatus, %0" : : "r" (x));
0530 }
0531 
0532 // machine exception program counter, holds the
0533 // instruction address to which a return from
0534 // exception will go.
0535 static inline void
0536 w_mepc(uint64 x)
0537 {
0538   asm volatile("csrw mepc, %0" : : "r" (x));
0539 }
0540 
0541 
0542 
0543 
0544 
0545 
0546 
0547 
0548 
0549 
0550 // Supervisor Status Register, sstatus
0551 
0552 #define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
0553 #define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
0554 #define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
0555 #define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
0556 #define SSTATUS_UIE (1L << 0)  // User Interrupt Enable
0557 
0558 static inline uint64
0559 r_sstatus()
0560 {
0561   uint64 x;
0562   asm volatile("csrr %0, sstatus" : "=r" (x) );
0563   return x;
0564 }
0565 
0566 static inline void
0567 w_sstatus(uint64 x)
0568 {
0569   asm volatile("csrw sstatus, %0" : : "r" (x));
0570 }
0571 
0572 // Supervisor Interrupt Pending
0573 static inline uint64
0574 r_sip()
0575 {
0576   uint64 x;
0577   asm volatile("csrr %0, sip" : "=r" (x) );
0578   return x;
0579 }
0580 
0581 static inline void
0582 w_sip(uint64 x)
0583 {
0584   asm volatile("csrw sip, %0" : : "r" (x));
0585 }
0586 
0587 // Supervisor Interrupt Enable
0588 #define SIE_SEIE (1L << 9) // external
0589 #define SIE_STIE (1L << 5) // timer
0590 static inline uint64
0591 r_sie()
0592 {
0593   uint64 x;
0594   asm volatile("csrr %0, sie" : "=r" (x) );
0595   return x;
0596 }
0597 
0598 
0599 
0600 static inline void
0601 w_sie(uint64 x)
0602 {
0603   asm volatile("csrw sie, %0" : : "r" (x));
0604 }
0605 
0606 // Machine-mode Interrupt Enable
0607 #define MIE_STIE (1L << 5)  // supervisor timer
0608 static inline uint64
0609 r_mie()
0610 {
0611   uint64 x;
0612   asm volatile("csrr %0, mie" : "=r" (x) );
0613   return x;
0614 }
0615 
0616 static inline void
0617 w_mie(uint64 x)
0618 {
0619   asm volatile("csrw mie, %0" : : "r" (x));
0620 }
0621 
0622 // supervisor exception program counter, holds the
0623 // instruction address to which a return from
0624 // exception will go.
0625 static inline void
0626 w_sepc(uint64 x)
0627 {
0628   asm volatile("csrw sepc, %0" : : "r" (x));
0629 }
0630 
0631 static inline uint64
0632 r_sepc()
0633 {
0634   uint64 x;
0635   asm volatile("csrr %0, sepc" : "=r" (x) );
0636   return x;
0637 }
0638 
0639 // Machine Exception Delegation
0640 static inline uint64
0641 r_medeleg()
0642 {
0643   uint64 x;
0644   asm volatile("csrr %0, medeleg" : "=r" (x) );
0645   return x;
0646 }
0647 
0648 
0649 
0650 static inline void
0651 w_medeleg(uint64 x)
0652 {
0653   asm volatile("csrw medeleg, %0" : : "r" (x));
0654 }
0655 
0656 // Machine Interrupt Delegation
0657 static inline uint64
0658 r_mideleg()
0659 {
0660   uint64 x;
0661   asm volatile("csrr %0, mideleg" : "=r" (x) );
0662   return x;
0663 }
0664 
0665 static inline void
0666 w_mideleg(uint64 x)
0667 {
0668   asm volatile("csrw mideleg, %0" : : "r" (x));
0669 }
0670 
0671 // Supervisor Trap-Vector Base Address
0672 // low two bits are mode.
0673 static inline void
0674 w_stvec(uint64 x)
0675 {
0676   asm volatile("csrw stvec, %0" : : "r" (x));
0677 }
0678 
0679 static inline uint64
0680 r_stvec()
0681 {
0682   uint64 x;
0683   asm volatile("csrr %0, stvec" : "=r" (x) );
0684   return x;
0685 }
0686 
0687 // Supervisor Timer Comparison Register
0688 static inline uint64
0689 r_stimecmp()
0690 {
0691   uint64 x;
0692   // asm volatile("csrr %0, stimecmp" : "=r" (x) );
0693   asm volatile("csrr %0, 0x14d" : "=r" (x) );
0694   return x;
0695 }
0696 
0697 
0698 
0699 
0700 static inline void
0701 w_stimecmp(uint64 x)
0702 {
0703   // asm volatile("csrw stimecmp, %0" : : "r" (x));
0704   asm volatile("csrw 0x14d, %0" : : "r" (x));
0705 }
0706 
0707 // Machine Environment Configuration Register
0708 static inline uint64
0709 r_menvcfg()
0710 {
0711   uint64 x;
0712   // asm volatile("csrr %0, menvcfg" : "=r" (x) );
0713   asm volatile("csrr %0, 0x30a" : "=r" (x) );
0714   return x;
0715 }
0716 
0717 static inline void
0718 w_menvcfg(uint64 x)
0719 {
0720   // asm volatile("csrw menvcfg, %0" : : "r" (x));
0721   asm volatile("csrw 0x30a, %0" : : "r" (x));
0722 }
0723 
0724 // Physical Memory Protection
0725 static inline void
0726 w_pmpcfg0(uint64 x)
0727 {
0728   asm volatile("csrw pmpcfg0, %0" : : "r" (x));
0729 }
0730 
0731 static inline void
0732 w_pmpaddr0(uint64 x)
0733 {
0734   asm volatile("csrw pmpaddr0, %0" : : "r" (x));
0735 }
0736 
0737 // use riscv's sv39 page table scheme.
0738 #define SATP_SV39 (8L << 60)
0739 
0740 #define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))
0741 
0742 // supervisor address translation and protection;
0743 // holds the address of the page table.
0744 static inline void
0745 w_satp(uint64 x)
0746 {
0747   asm volatile("csrw satp, %0" : : "r" (x));
0748 }
0749 
0750 static inline uint64
0751 r_satp()
0752 {
0753   uint64 x;
0754   asm volatile("csrr %0, satp" : "=r" (x) );
0755   return x;
0756 }
0757 
0758 // Supervisor Trap Cause
0759 static inline uint64
0760 r_scause()
0761 {
0762   uint64 x;
0763   asm volatile("csrr %0, scause" : "=r" (x) );
0764   return x;
0765 }
0766 
0767 // Supervisor Trap Value
0768 static inline uint64
0769 r_stval()
0770 {
0771   uint64 x;
0772   asm volatile("csrr %0, stval" : "=r" (x) );
0773   return x;
0774 }
0775 
0776 // Machine-mode Counter-Enable
0777 static inline void
0778 w_mcounteren(uint64 x)
0779 {
0780   asm volatile("csrw mcounteren, %0" : : "r" (x));
0781 }
0782 
0783 static inline uint64
0784 r_mcounteren()
0785 {
0786   uint64 x;
0787   asm volatile("csrr %0, mcounteren" : "=r" (x) );
0788   return x;
0789 }
0790 
0791 // machine-mode cycle counter
0792 static inline uint64
0793 r_time()
0794 {
0795   uint64 x;
0796   asm volatile("csrr %0, time" : "=r" (x) );
0797   return x;
0798 }
0799 
0800 // enable device interrupts
0801 static inline void
0802 intr_on()
0803 {
0804   w_sstatus(r_sstatus() | SSTATUS_SIE);
0805 }
0806 
0807 // disable device interrupts
0808 static inline void
0809 intr_off()
0810 {
0811   w_sstatus(r_sstatus() & ~SSTATUS_SIE);
0812 }
0813 
0814 // are device interrupts enabled?
0815 static inline int
0816 intr_get()
0817 {
0818   uint64 x = r_sstatus();
0819   return (x & SSTATUS_SIE) != 0;
0820 }
0821 
0822 static inline uint64
0823 r_sp()
0824 {
0825   uint64 x;
0826   asm volatile("mv %0, sp" : "=r" (x) );
0827   return x;
0828 }
0829 
0830 // read and write tp, the thread pointer, which xv6 uses to hold
0831 // this core's hartid (core number), the index into cpus[].
0832 static inline uint64
0833 r_tp()
0834 {
0835   uint64 x;
0836   asm volatile("mv %0, tp" : "=r" (x) );
0837   return x;
0838 }
0839 
0840 static inline void
0841 w_tp(uint64 x)
0842 {
0843   asm volatile("mv tp, %0" : : "r" (x));
0844 }
0845 
0846 
0847 
0848 
0849 
0850 static inline uint64
0851 r_ra()
0852 {
0853   uint64 x;
0854   asm volatile("mv %0, ra" : "=r" (x) );
0855   return x;
0856 }
0857 
0858 // flush the TLB.
0859 static inline void
0860 sfence_vma()
0861 {
0862   // the zero, zero means flush all TLB entries.
0863   asm volatile("sfence.vma zero, zero");
0864 }
0865 
0866 typedef uint64 pte_t;
0867 typedef uint64 *pagetable_t; // 512 PTEs
0868 
0869 #endif // __ASSEMBLER__
0870 
0871 #define PGSIZE 4096 // bytes per page
0872 #define PGSHIFT 12  // bits of offset within a page
0873 
0874 #define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
0875 #define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))
0876 
0877 #define PTE_V (1L << 0) // valid
0878 #define PTE_R (1L << 1)
0879 #define PTE_W (1L << 2)
0880 #define PTE_X (1L << 3)
0881 #define PTE_U (1L << 4) // user can access
0882 
0883 // shift a physical address to the right place for a PTE.
0884 #define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
0885 
0886 #define PTE2PA(pte) (((pte) >> 10) << 12)
0887 
0888 #define PTE_FLAGS(pte) ((pte) & 0x3FF)
0889 
0890 // extract the three 9-bit page table indices from a virtual address.
0891 #define PXMASK          0x1FF // 9 bits
0892 #define PXSHIFT(level)  (PGSHIFT+(9*(level)))
0893 #define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)
0894 
0895 // one beyond the highest possible virtual address.
0896 // MAXVA is actually one bit less than the max allowed by
0897 // Sv39, to avoid having to sign-extend virtual addresses
0898 // that have the high bit set.
0899 #define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

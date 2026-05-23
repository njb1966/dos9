3750 #include "types.h"
3751 #include "riscv.h"
3752 #include "defs.h"
3753 #include "param.h"
3754 #include "memlayout.h"
3755 #include "spinlock.h"
3756 #include "proc.h"
3757 #include "vm.h"
3758 
3759 uint64
3760 sys_exit(void)
3761 {
3762   int n;
3763   argint(0, &n);
3764   kexit(n);
3765   return 0;  // not reached
3766 }
3767 
3768 uint64
3769 sys_getpid(void)
3770 {
3771   return myproc()->pid;
3772 }
3773 
3774 uint64
3775 sys_fork(void)
3776 {
3777   return kfork();
3778 }
3779 
3780 uint64
3781 sys_wait(void)
3782 {
3783   uint64 p;
3784   argaddr(0, &p);
3785   return kwait(p);
3786 }
3787 
3788 
3789 
3790 
3791 
3792 
3793 
3794 
3795 
3796 
3797 
3798 
3799 
3800 uint64
3801 sys_sbrk(void)
3802 {
3803   uint64 addr;
3804   int t;
3805   int n;
3806 
3807   argint(0, &n);
3808   argint(1, &t);
3809   addr = myproc()->sz;
3810 
3811   if(t == SBRK_EAGER || n < 0) {
3812     if(growproc(n) < 0) {
3813       return -1;
3814     }
3815   } else {
3816     // Lazily allocate memory for this process: increase its memory
3817     // size but don't allocate memory. If the processes uses the
3818     // memory, vmfault() will allocate it.
3819     if(addr + n < addr)
3820       return -1;
3821     if(addr + n > TRAPFRAME)
3822       return -1;
3823     myproc()->sz += n;
3824   }
3825   return addr;
3826 }
3827 
3828 uint64
3829 sys_pause(void)
3830 {
3831   int n;
3832   uint ticks0;
3833 
3834   argint(0, &n);
3835   if(n < 0)
3836     n = 0;
3837   acquire(&tickslock);
3838   ticks0 = ticks;
3839   while(ticks - ticks0 < n){
3840     if(killed(myproc())){
3841       release(&tickslock);
3842       return -1;
3843     }
3844     sleep(&ticks, &tickslock);
3845   }
3846   release(&tickslock);
3847   return 0;
3848 }
3849 
3850 uint64
3851 sys_kill(void)
3852 {
3853   int pid;
3854 
3855   argint(0, &pid);
3856   return kkill(pid);
3857 }
3858 
3859 // return how many clock tick interrupts have occurred
3860 // since start.
3861 uint64
3862 sys_uptime(void)
3863 {
3864   uint xticks;
3865 
3866   acquire(&tickslock);
3867   xticks = ticks;
3868   release(&tickslock);
3869   return xticks;
3870 }
3871 
3872 
3873 
3874 
3875 
3876 
3877 
3878 
3879 
3880 
3881 
3882 
3883 
3884 
3885 
3886 
3887 
3888 
3889 
3890 
3891 
3892 
3893 
3894 
3895 
3896 
3897 
3898 
3899 

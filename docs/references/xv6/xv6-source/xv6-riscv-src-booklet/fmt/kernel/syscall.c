3600 #include "types.h"
3601 #include "param.h"
3602 #include "memlayout.h"
3603 #include "riscv.h"
3604 #include "spinlock.h"
3605 #include "proc.h"
3606 #include "syscall.h"
3607 #include "defs.h"
3608 
3609 // Fetch the uint64 at addr from the current process.
3610 int
3611 fetchaddr(uint64 addr, uint64 *ip)
3612 {
3613   struct proc *p = myproc();
3614   if(addr >= p->sz || addr+sizeof(uint64) > p->sz) // both tests needed, in case of overflow
3615     return -1;
3616   if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
3617     return -1;
3618   return 0;
3619 }
3620 
3621 // Fetch the nul-terminated string at addr from the current process.
3622 // Returns length of string, not including nul, or -1 for error.
3623 int
3624 fetchstr(uint64 addr, char *buf, int max)
3625 {
3626   struct proc *p = myproc();
3627   if(copyinstr(p->pagetable, buf, addr, max) < 0)
3628     return -1;
3629   return strlen(buf);
3630 }
3631 
3632 static uint64
3633 argraw(int n)
3634 {
3635   struct proc *p = myproc();
3636   switch (n) {
3637   case 0:
3638     return p->trapframe->a0;
3639   case 1:
3640     return p->trapframe->a1;
3641   case 2:
3642     return p->trapframe->a2;
3643   case 3:
3644     return p->trapframe->a3;
3645   case 4:
3646     return p->trapframe->a4;
3647   case 5:
3648     return p->trapframe->a5;
3649   }
3650   panic("argraw");
3651   return -1;
3652 }
3653 
3654 // Fetch the nth 32-bit system call argument.
3655 void
3656 argint(int n, int *ip)
3657 {
3658   *ip = argraw(n);
3659 }
3660 
3661 // Retrieve an argument as a pointer.
3662 // Doesn't check for legality, since
3663 // copyin/copyout will do that.
3664 void
3665 argaddr(int n, uint64 *ip)
3666 {
3667   *ip = argraw(n);
3668 }
3669 
3670 // Fetch the nth word-sized system call argument as a null-terminated string.
3671 // Copies into buf, at most max.
3672 // Returns string length if OK (including nul), -1 if error.
3673 int
3674 argstr(int n, char *buf, int max)
3675 {
3676   uint64 addr;
3677   argaddr(n, &addr);
3678   return fetchstr(addr, buf, max);
3679 }
3680 
3681 // Prototypes for the functions that handle system calls.
3682 extern uint64 sys_fork(void);
3683 extern uint64 sys_exit(void);
3684 extern uint64 sys_wait(void);
3685 extern uint64 sys_pipe(void);
3686 extern uint64 sys_read(void);
3687 extern uint64 sys_kill(void);
3688 extern uint64 sys_exec(void);
3689 extern uint64 sys_fstat(void);
3690 extern uint64 sys_chdir(void);
3691 extern uint64 sys_dup(void);
3692 extern uint64 sys_getpid(void);
3693 extern uint64 sys_sbrk(void);
3694 extern uint64 sys_pause(void);
3695 extern uint64 sys_uptime(void);
3696 extern uint64 sys_open(void);
3697 extern uint64 sys_write(void);
3698 extern uint64 sys_mknod(void);
3699 extern uint64 sys_unlink(void);
3700 extern uint64 sys_link(void);
3701 extern uint64 sys_mkdir(void);
3702 extern uint64 sys_close(void);
3703 
3704 // An array mapping syscall numbers from syscall.h
3705 // to the function that handles the system call.
3706 static uint64 (*syscalls[])(void) = {
3707 [SYS_fork]    sys_fork,
3708 [SYS_exit]    sys_exit,
3709 [SYS_wait]    sys_wait,
3710 [SYS_pipe]    sys_pipe,
3711 [SYS_read]    sys_read,
3712 [SYS_kill]    sys_kill,
3713 [SYS_exec]    sys_exec,
3714 [SYS_fstat]   sys_fstat,
3715 [SYS_chdir]   sys_chdir,
3716 [SYS_dup]     sys_dup,
3717 [SYS_getpid]  sys_getpid,
3718 [SYS_sbrk]    sys_sbrk,
3719 [SYS_pause]   sys_pause,
3720 [SYS_uptime]  sys_uptime,
3721 [SYS_open]    sys_open,
3722 [SYS_write]   sys_write,
3723 [SYS_mknod]   sys_mknod,
3724 [SYS_unlink]  sys_unlink,
3725 [SYS_link]    sys_link,
3726 [SYS_mkdir]   sys_mkdir,
3727 [SYS_close]   sys_close,
3728 };
3729 
3730 void
3731 syscall(void)
3732 {
3733   int num;
3734   struct proc *p = myproc();
3735 
3736   num = p->trapframe->a7;
3737   if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
3738     // Use num to lookup the system call function for num, call it,
3739     // and store its return value in p->trapframe->a0
3740     p->trapframe->a0 = syscalls[num]();
3741   } else {
3742     printf("%d %s: unknown sys call %d\n",
3743             p->pid, p->name, num);
3744     p->trapframe->a0 = -1;
3745   }
3746 }
3747 
3748 
3749 

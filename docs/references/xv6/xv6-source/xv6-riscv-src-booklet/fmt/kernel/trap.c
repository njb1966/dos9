3300 #include "types.h"
3301 #include "param.h"
3302 #include "memlayout.h"
3303 #include "riscv.h"
3304 #include "spinlock.h"
3305 #include "proc.h"
3306 #include "defs.h"
3307 
3308 struct spinlock tickslock;
3309 uint ticks;
3310 
3311 extern char trampoline[], uservec[];
3312 
3313 // in kernelvec.S, calls kerneltrap().
3314 void kernelvec();
3315 
3316 extern int devintr();
3317 
3318 void
3319 trapinit(void)
3320 {
3321   initlock(&tickslock, "time");
3322 }
3323 
3324 // set up to take exceptions and traps while in the kernel.
3325 void
3326 trapinithart(void)
3327 {
3328   w_stvec((uint64)kernelvec);
3329 }
3330 
3331 //
3332 // handle an interrupt, exception, or system call from user space.
3333 // called from, and returns to, trampoline.S
3334 // return value is user satp for trampoline.S to switch to.
3335 //
3336 uint64
3337 usertrap(void)
3338 {
3339   int which_dev = 0;
3340 
3341   if((r_sstatus() & SSTATUS_SPP) != 0)
3342     panic("usertrap: not from user mode");
3343 
3344   // send interrupts and exceptions to kerneltrap(),
3345   // since we're now in the kernel.
3346   w_stvec((uint64)kernelvec);  //DOC: kernelvec
3347 
3348   struct proc *p = myproc();
3349 
3350   // save user program counter.
3351   p->trapframe->epc = r_sepc();
3352 
3353   if(r_scause() == 8){
3354     // system call
3355 
3356     if(killed(p))
3357       kexit(-1);
3358 
3359     // sepc points to the ecall instruction,
3360     // but we want to return to the next instruction.
3361     p->trapframe->epc += 4;
3362 
3363     // an interrupt will change sepc, scause, and sstatus,
3364     // so enable only now that we're done with those registers.
3365     intr_on();
3366 
3367     syscall();
3368   } else if((which_dev = devintr()) != 0){
3369     // ok
3370   } else if((r_scause() == 15 || r_scause() == 13) &&
3371             vmfault(p->pagetable, r_stval(), (r_scause() == 13)? 1 : 0) != 0) {
3372     // page fault on lazily-allocated page
3373   } else {
3374     printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
3375     printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
3376     setkilled(p);
3377   }
3378 
3379   if(killed(p))
3380     kexit(-1);
3381 
3382   // give up the CPU if this is a timer interrupt.
3383   if(which_dev == 2)
3384     yield();
3385 
3386   prepare_return();
3387 
3388   // the user page table to switch to, for trampoline.S
3389   uint64 satp = MAKE_SATP(p->pagetable);
3390 
3391   // return to trampoline.S; satp value in a0.
3392   return satp;
3393 }
3394 
3395 
3396 
3397 
3398 
3399 
3400 //
3401 // set up trapframe and control registers for a return to user space
3402 //
3403 void
3404 prepare_return(void)
3405 {
3406   struct proc *p = myproc();
3407 
3408   // we're about to switch the destination of traps from
3409   // kerneltrap() to usertrap(). because a trap from kernel
3410   // code to usertrap would be a disaster, turn off interrupts.
3411   intr_off();
3412 
3413   // send syscalls, interrupts, and exceptions to uservec in trampoline.S
3414   uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
3415   w_stvec(trampoline_uservec);
3416 
3417   // set up trapframe values that uservec will need when
3418   // the process next traps into the kernel.
3419   p->trapframe->kernel_satp = r_satp();         // kernel page table
3420   p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
3421   p->trapframe->kernel_trap = (uint64)usertrap;
3422   p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()
3423 
3424   // set up the registers that trampoline.S's sret will use
3425   // to get to user space.
3426 
3427   // set S Previous Privilege mode to User.
3428   unsigned long x = r_sstatus();
3429   x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
3430   x |= SSTATUS_SPIE; // enable interrupts in user mode
3431   w_sstatus(x);
3432 
3433   // set S Exception Program Counter to the saved user pc.
3434   w_sepc(p->trapframe->epc);
3435 }
3436 
3437 
3438 
3439 
3440 
3441 
3442 
3443 
3444 
3445 
3446 
3447 
3448 
3449 
3450 // interrupts and exceptions from kernel code go here via kernelvec,
3451 // on whatever the current kernel stack is.
3452 void
3453 kerneltrap()
3454 {
3455   int which_dev = 0;
3456   uint64 sepc = r_sepc();
3457   uint64 sstatus = r_sstatus();
3458   uint64 scause = r_scause();
3459 
3460   if((sstatus & SSTATUS_SPP) == 0)
3461     panic("kerneltrap: not from supervisor mode");
3462   if(intr_get() != 0)
3463     panic("kerneltrap: interrupts enabled");
3464 
3465   if((which_dev = devintr()) == 0){
3466     // interrupt or trap from an unknown source
3467     printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
3468     panic("kerneltrap");
3469   }
3470 
3471   // give up the CPU if this is a timer interrupt.
3472   if(which_dev == 2 && myproc() != 0)
3473     yield();
3474 
3475   // the yield() may have caused some traps to occur,
3476   // so restore trap registers for use by kernelvec.S's sepc instruction.
3477   w_sepc(sepc);
3478   w_sstatus(sstatus);
3479 }
3480 
3481 void
3482 clockintr()
3483 {
3484   if(cpuid() == 0){
3485     acquire(&tickslock);
3486     ticks++;
3487     wakeup(&ticks);
3488     release(&tickslock);
3489   }
3490 
3491   // ask for the next timer interrupt. this also clears
3492   // the interrupt request. 1000000 is about a tenth
3493   // of a second.
3494   w_stimecmp(r_time() + 1000000);
3495 }
3496 
3497 
3498 
3499 
3500 // check if it's an external interrupt or software interrupt,
3501 // and handle it.
3502 // returns 2 if timer interrupt,
3503 // 1 if other device,
3504 // 0 if not recognized.
3505 int
3506 devintr()
3507 {
3508   uint64 scause = r_scause();
3509 
3510   if(scause == 0x8000000000000009L){
3511     // this is a supervisor external interrupt, via PLIC.
3512 
3513     // irq indicates which device interrupted.
3514     int irq = plic_claim();
3515 
3516     if(irq == UART0_IRQ){
3517       uartintr();
3518     } else if(irq == VIRTIO0_IRQ){
3519       virtio_disk_intr();
3520     } else if(irq){
3521       printf("unexpected interrupt irq=%d\n", irq);
3522     }
3523 
3524     // the PLIC allows each device to raise at most one
3525     // interrupt at a time; tell the PLIC the device is
3526     // now allowed to interrupt again.
3527     if(irq)
3528       plic_complete(irq);
3529 
3530     return 1;
3531   } else if(scause == 0x8000000000000005L){
3532     // timer interrupt.
3533     clockintr();
3534     return 2;
3535   } else {
3536     return 0;
3537   }
3538 }
3539 
3540 
3541 
3542 
3543 
3544 
3545 
3546 
3547 
3548 
3549 

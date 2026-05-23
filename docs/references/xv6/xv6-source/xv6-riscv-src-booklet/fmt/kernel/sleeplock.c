4450 // Sleeping locks
4451 
4452 #include "types.h"
4453 #include "riscv.h"
4454 #include "defs.h"
4455 #include "param.h"
4456 #include "memlayout.h"
4457 #include "spinlock.h"
4458 #include "proc.h"
4459 #include "sleeplock.h"
4460 
4461 void
4462 initsleeplock(struct sleeplock *lk, char *name)
4463 {
4464   initlock(&lk->lk, "sleep lock");
4465   lk->name = name;
4466   lk->locked = 0;
4467   lk->pid = 0;
4468 }
4469 
4470 void
4471 acquiresleep(struct sleeplock *lk)
4472 {
4473   acquire(&lk->lk);
4474   while (lk->locked) {
4475     sleep(lk, &lk->lk);
4476   }
4477   lk->locked = 1;
4478   lk->pid = myproc()->pid;
4479   release(&lk->lk);
4480 }
4481 
4482 void
4483 releasesleep(struct sleeplock *lk)
4484 {
4485   acquire(&lk->lk);
4486   lk->locked = 0;
4487   lk->pid = 0;
4488   wakeup(lk);
4489   release(&lk->lk);
4490 }
4491 
4492 
4493 
4494 
4495 
4496 
4497 
4498 
4499 
4500 int
4501 holdingsleep(struct sleeplock *lk)
4502 {
4503   int r;
4504 
4505   acquire(&lk->lk);
4506   r = lk->locked && (lk->pid == myproc()->pid);
4507   release(&lk->lk);
4508   return r;
4509 }
4510 
4511 
4512 
4513 
4514 
4515 
4516 
4517 
4518 
4519 
4520 
4521 
4522 
4523 
4524 
4525 
4526 
4527 
4528 
4529 
4530 
4531 
4532 
4533 
4534 
4535 
4536 
4537 
4538 
4539 
4540 
4541 
4542 
4543 
4544 
4545 
4546 
4547 
4548 
4549 

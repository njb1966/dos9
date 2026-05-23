0300 struct buf;
0301 struct context;
0302 struct file;
0303 struct inode;
0304 struct pipe;
0305 struct proc;
0306 struct spinlock;
0307 struct sleeplock;
0308 struct stat;
0309 struct superblock;
0310 
0311 // bio.c
0312 void            binit(void);
0313 struct buf*     bread(uint, uint);
0314 void            brelse(struct buf*);
0315 void            bwrite(struct buf*);
0316 void            bpin(struct buf*);
0317 void            bunpin(struct buf*);
0318 
0319 // console.c
0320 void            consoleinit(void);
0321 void            consoleintr(int);
0322 void            consputc(int);
0323 
0324 // exec.c
0325 int             kexec(char*, char**);
0326 
0327 // file.c
0328 struct file*    filealloc(void);
0329 void            fileclose(struct file*);
0330 struct file*    filedup(struct file*);
0331 void            fileinit(void);
0332 int             fileread(struct file*, uint64, int n);
0333 int             filestat(struct file*, uint64 addr);
0334 int             filewrite(struct file*, uint64, int n);
0335 
0336 // fs.c
0337 void            fsinit(int);
0338 int             dirlink(struct inode*, char*, uint);
0339 struct inode*   dirlookup(struct inode*, char*, uint*);
0340 struct inode*   ialloc(uint, short);
0341 struct inode*   idup(struct inode*);
0342 void            iinit();
0343 void            ilock(struct inode*);
0344 void            iput(struct inode*);
0345 void            iunlock(struct inode*);
0346 void            iunlockput(struct inode*);
0347 void            iupdate(struct inode*);
0348 int             namecmp(const char*, const char*);
0349 struct inode*   namei(char*);
0350 struct inode*   nameiparent(char*, char*);
0351 int             readi(struct inode*, int, uint64, uint, uint);
0352 void            stati(struct inode*, struct stat*);
0353 int             writei(struct inode*, int, uint64, uint, uint);
0354 void            itrunc(struct inode*);
0355 void            ireclaim(int);
0356 
0357 // kalloc.c
0358 void*           kalloc(void);
0359 void            kfree(void *);
0360 void            kinit(void);
0361 
0362 // log.c
0363 void            initlog(int, struct superblock*);
0364 void            log_write(struct buf*);
0365 void            begin_op(void);
0366 void            end_op(void);
0367 
0368 // pipe.c
0369 int             pipealloc(struct file**, struct file**);
0370 void            pipeclose(struct pipe*, int);
0371 int             piperead(struct pipe*, uint64, int);
0372 int             pipewrite(struct pipe*, uint64, int);
0373 
0374 // printf.c
0375 int             printf(char*, ...) __attribute__ ((format (printf, 1, 2)));
0376 void            panic(char*) __attribute__((noreturn));
0377 void            printfinit(void);
0378 
0379 // proc.c
0380 int             cpuid(void);
0381 void            kexit(int);
0382 int             kfork(void);
0383 int             growproc(int);
0384 void            proc_mapstacks(pagetable_t);
0385 pagetable_t     proc_pagetable(struct proc *);
0386 void            proc_freepagetable(pagetable_t, uint64);
0387 int             kkill(int);
0388 int             killed(struct proc*);
0389 void            setkilled(struct proc*);
0390 struct cpu*     mycpu(void);
0391 struct proc*    myproc();
0392 void            procinit(void);
0393 void            scheduler(void) __attribute__((noreturn));
0394 void            sched(void);
0395 void            sleep(void*, struct spinlock*);
0396 void            userinit(void);
0397 int             kwait(uint64);
0398 void            wakeup(void*);
0399 void            yield(void);
0400 int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
0401 int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
0402 void            procdump(void);
0403 
0404 // swtch.S
0405 void            swtch(struct context*, struct context*);
0406 
0407 // spinlock.c
0408 void            acquire(struct spinlock*);
0409 int             holding(struct spinlock*);
0410 void            initlock(struct spinlock*, char*);
0411 void            release(struct spinlock*);
0412 void            push_off(void);
0413 void            pop_off(void);
0414 
0415 // sleeplock.c
0416 void            acquiresleep(struct sleeplock*);
0417 void            releasesleep(struct sleeplock*);
0418 int             holdingsleep(struct sleeplock*);
0419 void            initsleeplock(struct sleeplock*, char*);
0420 
0421 // string.c
0422 int             memcmp(const void*, const void*, uint);
0423 void*           memmove(void*, const void*, uint);
0424 void*           memset(void*, int, uint);
0425 char*           safestrcpy(char*, const char*, int);
0426 int             strlen(const char*);
0427 int             strncmp(const char*, const char*, uint);
0428 char*           strncpy(char*, const char*, int);
0429 
0430 // syscall.c
0431 void            argint(int, int*);
0432 int             argstr(int, char*, int);
0433 void            argaddr(int, uint64 *);
0434 int             fetchstr(uint64, char*, int);
0435 int             fetchaddr(uint64, uint64*);
0436 void            syscall();
0437 
0438 // trap.c
0439 extern uint     ticks;
0440 void            trapinit(void);
0441 void            trapinithart(void);
0442 extern struct spinlock tickslock;
0443 void            prepare_return(void);
0444 
0445 // uart.c
0446 void            uartinit(void);
0447 void            uartintr(void);
0448 void            uartwrite(char [], int);
0449 void            uartputc_sync(int);
0450 int             uartgetc(void);
0451 
0452 // vm.c
0453 void            kvminit(void);
0454 void            kvminithart(void);
0455 void            kvmmap(pagetable_t, uint64, uint64, uint64, int);
0456 int             mappages(pagetable_t, uint64, uint64, uint64, int);
0457 pagetable_t     uvmcreate(void);
0458 uint64          uvmalloc(pagetable_t, uint64, uint64, int);
0459 uint64          uvmdealloc(pagetable_t, uint64, uint64);
0460 int             uvmcopy(pagetable_t, pagetable_t, uint64);
0461 void            uvmfree(pagetable_t, uint64);
0462 void            uvmunmap(pagetable_t, uint64, uint64, int);
0463 void            uvmclear(pagetable_t, uint64);
0464 pte_t *         walk(pagetable_t, uint64, int);
0465 uint64          walkaddr(pagetable_t, uint64);
0466 int             copyout(pagetable_t, uint64, char *, uint64);
0467 int             copyin(pagetable_t, char *, uint64, uint64);
0468 int             copyinstr(pagetable_t, char *, uint64, uint64);
0469 int             ismapped(pagetable_t, uint64);
0470 uint64          vmfault(pagetable_t, uint64, int);
0471 
0472 // plic.c
0473 void            plicinit(void);
0474 void            plicinithart(void);
0475 int             plic_claim(void);
0476 void            plic_complete(int);
0477 
0478 // virtio_disk.c
0479 void            virtio_disk_init(void);
0480 void            virtio_disk_rw(struct buf *, int);
0481 void            virtio_disk_intr(void);
0482 
0483 // number of elements in fixed-size array
0484 #define NELEM(x) (sizeof(x)/sizeof((x)[0]))
0485 
0486 
0487 
0488 
0489 
0490 
0491 
0492 
0493 
0494 
0495 
0496 
0497 
0498 
0499 

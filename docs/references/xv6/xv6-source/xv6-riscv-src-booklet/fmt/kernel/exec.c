6400 #include "types.h"
6401 #include "param.h"
6402 #include "memlayout.h"
6403 #include "riscv.h"
6404 #include "spinlock.h"
6405 #include "proc.h"
6406 #include "defs.h"
6407 #include "elf.h"
6408 
6409 static int loadseg(pde_t *, uint64, struct inode *, uint, uint);
6410 
6411 // map ELF permissions to PTE permission bits.
6412 int flags2perm(int flags)
6413 {
6414     int perm = 0;
6415     if(flags & 0x1)
6416       perm = PTE_X;
6417     if(flags & 0x2)
6418       perm |= PTE_W;
6419     return perm;
6420 }
6421 
6422 //
6423 // the implementation of the exec() system call
6424 //
6425 int
6426 kexec(char *path, char **argv)
6427 {
6428   char *s, *last;
6429   int i, off;
6430   uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
6431   struct elfhdr elf;
6432   struct inode *ip;
6433   struct proghdr ph;
6434   pagetable_t pagetable = 0, oldpagetable;
6435   struct proc *p = myproc();
6436 
6437   begin_op();
6438 
6439   // Open the executable file.
6440   if((ip = namei(path)) == 0){
6441     end_op();
6442     return -1;
6443   }
6444   ilock(ip);
6445 
6446   // Read the ELF header.
6447   if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
6448     goto bad;
6449 
6450   // Is this really an ELF file?
6451   if(elf.magic != ELF_MAGIC)
6452     goto bad;
6453 
6454   if((pagetable = proc_pagetable(p)) == 0)
6455     goto bad;
6456 
6457   // Load program into memory.
6458   for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
6459     if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
6460       goto bad;
6461     if(ph.type != ELF_PROG_LOAD)
6462       continue;
6463     if(ph.memsz < ph.filesz)
6464       goto bad;
6465     if(ph.vaddr + ph.memsz < ph.vaddr)
6466       goto bad;
6467     if(ph.vaddr % PGSIZE != 0)
6468       goto bad;
6469     uint64 sz1;
6470     if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
6471       goto bad;
6472     sz = sz1;
6473     if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
6474       goto bad;
6475   }
6476   iunlockput(ip);
6477   end_op();
6478   ip = 0;
6479 
6480   p = myproc();
6481   uint64 oldsz = p->sz;
6482 
6483   // Allocate some pages at the next page boundary.
6484   // Make the first inaccessible as a stack guard.
6485   // Use the rest as the user stack.
6486   sz = PGROUNDUP(sz);
6487   uint64 sz1;
6488   if((sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK+1)*PGSIZE, PTE_W)) == 0)
6489     goto bad;
6490   sz = sz1;
6491   uvmclear(pagetable, sz-(USERSTACK+1)*PGSIZE);
6492   sp = sz;
6493   stackbase = sp - USERSTACK*PGSIZE;
6494 
6495   // Copy argument strings into new stack, remember their
6496   // addresses in ustack[].
6497   for(argc = 0; argv[argc]; argc++) {
6498     if(argc >= MAXARG)
6499       goto bad;
6500     sp -= strlen(argv[argc]) + 1;
6501     sp -= sp % 16; // riscv sp must be 16-byte aligned
6502     if(sp < stackbase)
6503       goto bad;
6504     if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
6505       goto bad;
6506     ustack[argc] = sp;
6507   }
6508   ustack[argc] = 0;
6509 
6510   // push a copy of ustack[], the array of argv[] pointers.
6511   sp -= (argc+1) * sizeof(uint64);
6512   sp -= sp % 16;
6513   if(sp < stackbase)
6514     goto bad;
6515   if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
6516     goto bad;
6517 
6518   // a0 and a1 contain arguments to user main(argc, argv)
6519   // argc is returned via the system call return
6520   // value, which goes in a0.
6521   p->trapframe->a1 = sp;
6522 
6523   // Save program name for debugging.
6524   for(last=s=path; *s; s++)
6525     if(*s == '/')
6526       last = s+1;
6527   safestrcpy(p->name, last, sizeof(p->name));
6528 
6529   // Commit to the user image.
6530   oldpagetable = p->pagetable;
6531   p->pagetable = pagetable;
6532   p->sz = sz;
6533   p->trapframe->epc = elf.entry;  // initial program counter = ulib.c:start()
6534   p->trapframe->sp = sp; // initial stack pointer
6535   proc_freepagetable(oldpagetable, oldsz);
6536 
6537   return argc; // this ends up in a0, the first argument to main(argc, argv)
6538 
6539  bad:
6540   if(pagetable)
6541     proc_freepagetable(pagetable, sz);
6542   if(ip){
6543     iunlockput(ip);
6544     end_op();
6545   }
6546   return -1;
6547 }
6548 
6549 
6550 // Load an ELF program segment into pagetable at virtual address va.
6551 // va must be page-aligned
6552 // and the pages from va to va+sz must already be mapped.
6553 // Returns 0 on success, -1 on failure.
6554 static int
6555 loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
6556 {
6557   uint i, n;
6558   uint64 pa;
6559 
6560   for(i = 0; i < sz; i += PGSIZE){
6561     pa = walkaddr(pagetable, va + i);
6562     if(pa == 0)
6563       panic("loadseg: address should exist");
6564     if(sz - i < PGSIZE)
6565       n = sz - i;
6566     else
6567       n = PGSIZE;
6568     if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
6569       return -1;
6570   }
6571 
6572   return 0;
6573 }
6574 
6575 
6576 
6577 
6578 
6579 
6580 
6581 
6582 
6583 
6584 
6585 
6586 
6587 
6588 
6589 
6590 
6591 
6592 
6593 
6594 
6595 
6596 
6597 
6598 
6599 

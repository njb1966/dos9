6600 #include "types.h"
6601 #include "riscv.h"
6602 #include "defs.h"
6603 #include "param.h"
6604 #include "spinlock.h"
6605 #include "proc.h"
6606 #include "fs.h"
6607 #include "sleeplock.h"
6608 #include "file.h"
6609 
6610 #define PIPESIZE 512
6611 
6612 struct pipe {
6613   struct spinlock lock;
6614   char data[PIPESIZE];
6615   uint nread;     // number of bytes read
6616   uint nwrite;    // number of bytes written
6617   int readopen;   // read fd is still open
6618   int writeopen;  // write fd is still open
6619 };
6620 
6621 int
6622 pipealloc(struct file **f0, struct file **f1)
6623 {
6624   struct pipe *pi;
6625 
6626   pi = 0;
6627   *f0 = *f1 = 0;
6628   if((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
6629     goto bad;
6630   if((pi = (struct pipe*)kalloc()) == 0)
6631     goto bad;
6632   pi->readopen = 1;
6633   pi->writeopen = 1;
6634   pi->nwrite = 0;
6635   pi->nread = 0;
6636   initlock(&pi->lock, "pipe");
6637   (*f0)->type = FD_PIPE;
6638   (*f0)->readable = 1;
6639   (*f0)->writable = 0;
6640   (*f0)->pipe = pi;
6641   (*f1)->type = FD_PIPE;
6642   (*f1)->readable = 0;
6643   (*f1)->writable = 1;
6644   (*f1)->pipe = pi;
6645   return 0;
6646 
6647 
6648 
6649 
6650  bad:
6651   if(pi)
6652     kfree((char*)pi);
6653   if(*f0)
6654     fileclose(*f0);
6655   if(*f1)
6656     fileclose(*f1);
6657   return -1;
6658 }
6659 
6660 void
6661 pipeclose(struct pipe *pi, int writable)
6662 {
6663   acquire(&pi->lock);
6664   if(writable){
6665     pi->writeopen = 0;
6666     wakeup(&pi->nread);
6667   } else {
6668     pi->readopen = 0;
6669     wakeup(&pi->nwrite);
6670   }
6671   if(pi->readopen == 0 && pi->writeopen == 0){
6672     release(&pi->lock);
6673     kfree((char*)pi);
6674   } else
6675     release(&pi->lock);
6676 }
6677 
6678 int
6679 pipewrite(struct pipe *pi, uint64 addr, int n)
6680 {
6681   int i = 0;
6682   struct proc *pr = myproc();
6683 
6684   acquire(&pi->lock);
6685   while(i < n){
6686     if(pi->readopen == 0 || killed(pr)){
6687       release(&pi->lock);
6688       return -1;
6689     }
6690     if(pi->nwrite == pi->nread + PIPESIZE){ //DOC: pipewrite-full
6691       wakeup(&pi->nread);
6692       sleep(&pi->nwrite, &pi->lock);
6693     } else {
6694       char ch;
6695       if(copyin(pr->pagetable, &ch, addr + i, 1) == -1)
6696         break;
6697       pi->data[pi->nwrite++ % PIPESIZE] = ch;
6698       i++;
6699     }
6700   }
6701   wakeup(&pi->nread);
6702   release(&pi->lock);
6703 
6704   return i;
6705 }
6706 
6707 int
6708 piperead(struct pipe *pi, uint64 addr, int n)
6709 {
6710   int i;
6711   struct proc *pr = myproc();
6712   char ch;
6713 
6714   acquire(&pi->lock);
6715   while(pi->nread == pi->nwrite && pi->writeopen){  //DOC: pipe-empty
6716     if(killed(pr)){
6717       release(&pi->lock);
6718       return -1;
6719     }
6720     sleep(&pi->nread, &pi->lock); //DOC: piperead-sleep
6721   }
6722   for(i = 0; i < n; i++){  //DOC: piperead-copy
6723     if(pi->nread == pi->nwrite)
6724       break;
6725     ch = pi->data[pi->nread % PIPESIZE];
6726     if(copyout(pr->pagetable, addr + i, &ch, 1) == -1) {
6727       if(i == 0)
6728         i = -1;
6729       break;
6730     }
6731     pi->nread++;
6732   }
6733   wakeup(&pi->nwrite);  //DOC: piperead-wakeup
6734   release(&pi->lock);
6735   return i;
6736 }
6737 
6738 
6739 
6740 
6741 
6742 
6743 
6744 
6745 
6746 
6747 
6748 
6749 

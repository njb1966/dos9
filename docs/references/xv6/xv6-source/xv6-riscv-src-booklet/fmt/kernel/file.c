5650 //
5651 // Support functions for system calls that involve file descriptors.
5652 //
5653 
5654 #include "types.h"
5655 #include "riscv.h"
5656 #include "defs.h"
5657 #include "param.h"
5658 #include "fs.h"
5659 #include "spinlock.h"
5660 #include "sleeplock.h"
5661 #include "file.h"
5662 #include "stat.h"
5663 #include "proc.h"
5664 
5665 struct devsw devsw[NDEV];
5666 struct {
5667   struct spinlock lock;
5668   struct file file[NFILE];
5669 } ftable;
5670 
5671 void
5672 fileinit(void)
5673 {
5674   initlock(&ftable.lock, "ftable");
5675 }
5676 
5677 // Allocate a file structure.
5678 struct file*
5679 filealloc(void)
5680 {
5681   struct file *f;
5682 
5683   acquire(&ftable.lock);
5684   for(f = ftable.file; f < ftable.file + NFILE; f++){
5685     if(f->ref == 0){
5686       f->ref = 1;
5687       release(&ftable.lock);
5688       return f;
5689     }
5690   }
5691   release(&ftable.lock);
5692   return 0;
5693 }
5694 
5695 
5696 
5697 
5698 
5699 
5700 // Increment ref count for file f.
5701 struct file*
5702 filedup(struct file *f)
5703 {
5704   acquire(&ftable.lock);
5705   if(f->ref < 1)
5706     panic("filedup");
5707   f->ref++;
5708   release(&ftable.lock);
5709   return f;
5710 }
5711 
5712 // Close file f.  (Decrement ref count, close when reaches 0.)
5713 void
5714 fileclose(struct file *f)
5715 {
5716   struct file ff;
5717 
5718   acquire(&ftable.lock);
5719   if(f->ref < 1)
5720     panic("fileclose");
5721   if(--f->ref > 0){
5722     release(&ftable.lock);
5723     return;
5724   }
5725   ff = *f;
5726   f->ref = 0;
5727   f->type = FD_NONE;
5728   release(&ftable.lock);
5729 
5730   if(ff.type == FD_PIPE){
5731     pipeclose(ff.pipe, ff.writable);
5732   } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
5733     begin_op();
5734     iput(ff.ip);
5735     end_op();
5736   }
5737 }
5738 
5739 
5740 
5741 
5742 
5743 
5744 
5745 
5746 
5747 
5748 
5749 
5750 // Get metadata about file f.
5751 // addr is a user virtual address, pointing to a struct stat.
5752 int
5753 filestat(struct file *f, uint64 addr)
5754 {
5755   struct proc *p = myproc();
5756   struct stat st;
5757 
5758   if(f->type == FD_INODE || f->type == FD_DEVICE){
5759     ilock(f->ip);
5760     stati(f->ip, &st);
5761     iunlock(f->ip);
5762     if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
5763       return -1;
5764     return 0;
5765   }
5766   return -1;
5767 }
5768 
5769 // Read from file f.
5770 // addr is a user virtual address.
5771 int
5772 fileread(struct file *f, uint64 addr, int n)
5773 {
5774   int r = 0;
5775 
5776   if(f->readable == 0)
5777     return -1;
5778 
5779   if(f->type == FD_PIPE){
5780     r = piperead(f->pipe, addr, n);
5781   } else if(f->type == FD_DEVICE){
5782     if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
5783       return -1;
5784     r = devsw[f->major].read(1, addr, n);
5785   } else if(f->type == FD_INODE){
5786     ilock(f->ip);
5787     if((r = readi(f->ip, 1, addr, f->off, n)) > 0)
5788       f->off += r;
5789     iunlock(f->ip);
5790   } else {
5791     panic("fileread");
5792   }
5793 
5794   return r;
5795 }
5796 
5797 
5798 
5799 
5800 // Write to file f.
5801 // addr is a user virtual address.
5802 int
5803 filewrite(struct file *f, uint64 addr, int n)
5804 {
5805   int r, ret = 0;
5806 
5807   if(f->writable == 0)
5808     return -1;
5809 
5810   if(f->type == FD_PIPE){
5811     ret = pipewrite(f->pipe, addr, n);
5812   } else if(f->type == FD_DEVICE){
5813     if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
5814       return -1;
5815     ret = devsw[f->major].write(1, addr, n);
5816   } else if(f->type == FD_INODE){
5817     // write a few blocks at a time to avoid exceeding
5818     // the maximum log transaction size, including
5819     // i-node, indirect block, allocation blocks,
5820     // and 2 blocks of slop for non-aligned writes.
5821     int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
5822     int i = 0;
5823     while(i < n){
5824       int n1 = n - i;
5825       if(n1 > max)
5826         n1 = max;
5827 
5828       begin_op();
5829       ilock(f->ip);
5830       if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
5831         f->off += r;
5832       iunlock(f->ip);
5833       end_op();
5834 
5835       if(r != n1){
5836         // error from writei
5837         break;
5838       }
5839       i += r;
5840     }
5841     ret = (i == n ? n : -1);
5842   } else {
5843     panic("filewrite");
5844   }
5845 
5846   return ret;
5847 }
5848 
5849 

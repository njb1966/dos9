4550 #include "types.h"
4551 #include "riscv.h"
4552 #include "defs.h"
4553 #include "param.h"
4554 #include "spinlock.h"
4555 #include "sleeplock.h"
4556 #include "fs.h"
4557 #include "buf.h"
4558 
4559 // Simple logging that allows concurrent FS system calls.
4560 //
4561 // A log transaction contains the updates of multiple FS system
4562 // calls. The logging system only commits when there are
4563 // no FS system calls active. Thus there is never
4564 // any reasoning required about whether a commit might
4565 // write an uncommitted system call's updates to disk.
4566 //
4567 // A system call should call begin_op()/end_op() to mark
4568 // its start and end. Usually begin_op() just increments
4569 // the count of in-progress FS system calls and returns.
4570 // But if it thinks the log is close to running out, it
4571 // sleeps until the last outstanding end_op() commits.
4572 //
4573 // The log is a physical re-do log containing disk blocks.
4574 // The on-disk log format:
4575 //   header block, containing block #s for block A, B, C, ...
4576 //   block A
4577 //   block B
4578 //   block C
4579 //   ...
4580 // Log appends are synchronous.
4581 
4582 // Contents of the header block, used for both the on-disk header block
4583 // and to keep track in memory of logged block# before commit.
4584 struct logheader {
4585   int n;
4586   int block[LOGBLOCKS];
4587 };
4588 
4589 struct log {
4590   struct spinlock lock;
4591   int start;
4592   int outstanding; // how many FS sys calls are executing.
4593   int committing;  // in commit(), please wait.
4594   int dev;
4595   struct logheader lh;
4596 };
4597 
4598 
4599 
4600 struct log log;
4601 
4602 static void recover_from_log(void);
4603 static void commit();
4604 
4605 void
4606 initlog(int dev, struct superblock *sb)
4607 {
4608   if (sizeof(struct logheader) >= BSIZE)
4609     panic("initlog: too big logheader");
4610 
4611   initlock(&log.lock, "log");
4612   log.start = sb->logstart;
4613   log.dev = dev;
4614   recover_from_log();
4615 }
4616 
4617 // Copy committed blocks from log to their home location
4618 static void
4619 install_trans(int recovering)
4620 {
4621   int tail;
4622 
4623   for (tail = 0; tail < log.lh.n; tail++) {
4624     if(recovering) {
4625       printf("recovering tail %d dst %d\n", tail, log.lh.block[tail]);
4626     }
4627     struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
4628     struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
4629     memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
4630     bwrite(dbuf);  // write dst to disk
4631     if(recovering == 0)
4632       bunpin(dbuf);
4633     brelse(lbuf);
4634     brelse(dbuf);
4635   }
4636 }
4637 
4638 
4639 
4640 
4641 
4642 
4643 
4644 
4645 
4646 
4647 
4648 
4649 
4650 // Read the log header from disk into the in-memory log header
4651 static void
4652 read_head(void)
4653 {
4654   struct buf *buf = bread(log.dev, log.start);
4655   struct logheader *lh = (struct logheader *) (buf->data);
4656   int i;
4657   log.lh.n = lh->n;
4658   for (i = 0; i < log.lh.n; i++) {
4659     log.lh.block[i] = lh->block[i];
4660   }
4661   brelse(buf);
4662 }
4663 
4664 // Write in-memory log header to disk.
4665 // This is the true point at which the
4666 // current transaction commits.
4667 static void
4668 write_head(void)
4669 {
4670   struct buf *buf = bread(log.dev, log.start);
4671   struct logheader *hb = (struct logheader *) (buf->data);
4672   int i;
4673   hb->n = log.lh.n;
4674   for (i = 0; i < log.lh.n; i++) {
4675     hb->block[i] = log.lh.block[i];
4676   }
4677   bwrite(buf);
4678   brelse(buf);
4679 }
4680 
4681 static void
4682 recover_from_log(void)
4683 {
4684   read_head();
4685   install_trans(1); // if committed, copy from log to disk
4686   log.lh.n = 0;
4687   write_head(); // clear the log
4688 }
4689 
4690 
4691 
4692 
4693 
4694 
4695 
4696 
4697 
4698 
4699 
4700 // called at the start of each FS system call.
4701 void
4702 begin_op(void)
4703 {
4704   acquire(&log.lock);
4705   while(1){
4706     if(log.committing){
4707       sleep(&log, &log.lock);
4708     } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGBLOCKS){
4709       // this op might exhaust log space; wait for commit.
4710       sleep(&log, &log.lock);
4711     } else {
4712       log.outstanding += 1;
4713       release(&log.lock);
4714       break;
4715     }
4716   }
4717 }
4718 
4719 // called at the end of each FS system call.
4720 // commits if this was the last outstanding operation.
4721 void
4722 end_op(void)
4723 {
4724   int do_commit = 0;
4725 
4726   acquire(&log.lock);
4727   log.outstanding -= 1;
4728   if(log.committing)
4729     panic("log.committing");
4730   if(log.outstanding == 0){
4731     do_commit = 1;
4732     log.committing = 1;
4733   } else {
4734     // begin_op() may be waiting for log space,
4735     // and decrementing log.outstanding has decreased
4736     // the amount of reserved space.
4737     wakeup(&log);
4738   }
4739   release(&log.lock);
4740 
4741   if(do_commit){
4742     // call commit w/o holding locks, since not allowed
4743     // to sleep with locks.
4744     commit();
4745     acquire(&log.lock);
4746     log.committing = 0;
4747     wakeup(&log);
4748     release(&log.lock);
4749   }
4750 }
4751 
4752 // Copy modified blocks from cache to log.
4753 static void
4754 write_log(void)
4755 {
4756   int tail;
4757 
4758   for (tail = 0; tail < log.lh.n; tail++) {
4759     struct buf *to = bread(log.dev, log.start+tail+1); // log block
4760     struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
4761     memmove(to->data, from->data, BSIZE);
4762     bwrite(to);  // write the log
4763     brelse(from);
4764     brelse(to);
4765   }
4766 }
4767 
4768 static void
4769 commit()
4770 {
4771   if (log.lh.n > 0) {
4772     write_log();     // Write modified blocks from cache to log
4773     write_head();    // Write header to disk -- the real commit
4774     install_trans(0); // Now install writes to home locations
4775     log.lh.n = 0;
4776     write_head();    // Erase the transaction from the log
4777   }
4778 }
4779 
4780 // Caller has modified b->data and is done with the buffer.
4781 // Record the block number and pin in the cache by increasing refcnt.
4782 // commit()/write_log() will do the disk write.
4783 //
4784 // log_write() replaces bwrite(); a typical use is:
4785 //   bp = bread(...)
4786 //   modify bp->data[]
4787 //   log_write(bp)
4788 //   brelse(bp)
4789 void
4790 log_write(struct buf *b)
4791 {
4792   int i;
4793 
4794   acquire(&log.lock);
4795   if (log.lh.n >= LOGBLOCKS)
4796     panic("too big a transaction");
4797   if (log.outstanding < 1)
4798     panic("log_write outside of trans");
4799 
4800   for (i = 0; i < log.lh.n; i++) {
4801     if (log.lh.block[i] == b->blockno)   // log absorption
4802       break;
4803   }
4804   log.lh.block[i] = b->blockno;
4805   if (i == log.lh.n) {  // Add new block to log?
4806     bpin(b);
4807     log.lh.n++;
4808   }
4809   release(&log.lock);
4810 }
4811 
4812 
4813 
4814 
4815 
4816 
4817 
4818 
4819 
4820 
4821 
4822 
4823 
4824 
4825 
4826 
4827 
4828 
4829 
4830 
4831 
4832 
4833 
4834 
4835 
4836 
4837 
4838 
4839 
4840 
4841 
4842 
4843 
4844 
4845 
4846 
4847 
4848 
4849 

4850 // File system implementation.  Five layers:
4851 //   + Blocks: allocator for raw disk blocks.
4852 //   + Log: crash recovery for multi-step updates.
4853 //   + Files: inode allocator, reading, writing, metadata.
4854 //   + Directories: inode with special contents (list of other inodes!)
4855 //   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
4856 //
4857 // This file contains the low-level file system manipulation
4858 // routines.  The (higher-level) system call implementations
4859 // are in sysfile.c.
4860 
4861 #include "types.h"
4862 #include "riscv.h"
4863 #include "defs.h"
4864 #include "param.h"
4865 #include "stat.h"
4866 #include "spinlock.h"
4867 #include "proc.h"
4868 #include "sleeplock.h"
4869 #include "fs.h"
4870 #include "buf.h"
4871 #include "file.h"
4872 
4873 #define min(a, b) ((a) < (b) ? (a) : (b))
4874 // there should be one superblock per disk device, but we run with
4875 // only one device
4876 struct superblock sb;
4877 
4878 // Read the super block.
4879 static void
4880 readsb(int dev, struct superblock *sb)
4881 {
4882   struct buf *bp;
4883 
4884   bp = bread(dev, 1);
4885   memmove(sb, bp->data, sizeof(*sb));
4886   brelse(bp);
4887 }
4888 
4889 // Init fs
4890 void
4891 fsinit(int dev) {
4892   readsb(dev, &sb);
4893   if(sb.magic != FSMAGIC)
4894     panic("invalid file system");
4895   initlog(dev, &sb);
4896   ireclaim(dev);
4897 }
4898 
4899 
4900 // Zero a block.
4901 static void
4902 bzero(int dev, int bno)
4903 {
4904   struct buf *bp;
4905 
4906   bp = bread(dev, bno);
4907   memset(bp->data, 0, BSIZE);
4908   log_write(bp);
4909   brelse(bp);
4910 }
4911 
4912 // Blocks.
4913 
4914 // Allocate a zeroed disk block.
4915 // returns 0 if out of disk space.
4916 static uint
4917 balloc(uint dev)
4918 {
4919   int b, bi, m;
4920   struct buf *bp;
4921 
4922   bp = 0;
4923   for(b = 0; b < sb.size; b += BPB){
4924     bp = bread(dev, BBLOCK(b, sb));
4925     for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
4926       m = 1 << (bi % 8);
4927       if((bp->data[bi/8] & m) == 0){  // Is block free?
4928         bp->data[bi/8] |= m;  // Mark block in use.
4929         log_write(bp);
4930         brelse(bp);
4931         bzero(dev, b + bi);
4932         return b + bi;
4933       }
4934     }
4935     brelse(bp);
4936   }
4937   printf("balloc: out of blocks\n");
4938   return 0;
4939 }
4940 
4941 
4942 
4943 
4944 
4945 
4946 
4947 
4948 
4949 
4950 // Free a disk block.
4951 static void
4952 bfree(int dev, uint b)
4953 {
4954   struct buf *bp;
4955   int bi, m;
4956 
4957   bp = bread(dev, BBLOCK(b, sb));
4958   bi = b % BPB;
4959   m = 1 << (bi % 8);
4960   if((bp->data[bi/8] & m) == 0)
4961     panic("freeing free block");
4962   bp->data[bi/8] &= ~m;
4963   log_write(bp);
4964   brelse(bp);
4965 }
4966 
4967 // Inodes.
4968 //
4969 // An inode describes a single unnamed file.
4970 // The inode disk structure holds metadata: the file's type,
4971 // its size, the number of links referring to it, and the
4972 // list of blocks holding the file's content.
4973 //
4974 // The inodes are laid out sequentially on disk at block
4975 // sb.inodestart. Each inode has a number, indicating its
4976 // position on the disk.
4977 //
4978 // The kernel keeps a table of in-use inodes in memory
4979 // to provide a place for synchronizing access
4980 // to inodes used by multiple processes. The in-memory
4981 // inodes include book-keeping information that is
4982 // not stored on disk: ip->ref and ip->valid.
4983 //
4984 // An inode and its in-memory representation go through a
4985 // sequence of states before they can be used by the
4986 // rest of the file system code.
4987 //
4988 // * Allocation: an inode is allocated if its type (on disk)
4989 //   is non-zero. ialloc() allocates, and iput() frees if
4990 //   the reference and link counts have fallen to zero.
4991 //
4992 // * Referencing in table: an entry in the inode table
4993 //   is free if ip->ref is zero. Otherwise ip->ref tracks
4994 //   the number of in-memory pointers to the entry (open
4995 //   files and current directories). iget() finds or
4996 //   creates a table entry and increments its ref; iput()
4997 //   decrements ref.
4998 //
4999 // * Valid: the information (type, size, &c) in an inode
5000 //   table entry is only correct when ip->valid is 1.
5001 //   ilock() reads the inode from
5002 //   the disk and sets ip->valid, while iput() clears
5003 //   ip->valid if ip->ref has fallen to zero.
5004 //
5005 // * Locked: file system code may only examine and modify
5006 //   the information in an inode and its content if it
5007 //   has first locked the inode.
5008 //
5009 // Thus a typical sequence is:
5010 //   ip = iget(dev, inum)
5011 //   ilock(ip)
5012 //   ... examine and modify ip->xxx ...
5013 //   iunlock(ip)
5014 //   iput(ip)
5015 //
5016 // ilock() is separate from iget() so that system calls can
5017 // get a long-term reference to an inode (as for an open file)
5018 // and only lock it for short periods (e.g., in read()).
5019 // The separation also helps avoid deadlock and races during
5020 // pathname lookup. iget() increments ip->ref so that the inode
5021 // stays in the table and pointers to it remain valid.
5022 //
5023 // Many internal file system functions expect the caller to
5024 // have locked the inodes involved; this lets callers create
5025 // multi-step atomic operations.
5026 //
5027 // The itable.lock spin-lock protects the allocation of itable
5028 // entries. Since ip->ref indicates whether an entry is free,
5029 // and ip->dev and ip->inum indicate which i-node an entry
5030 // holds, one must hold itable.lock while using any of those fields.
5031 //
5032 // An ip->lock sleep-lock protects all ip-> fields other than ref,
5033 // dev, and inum.  One must hold ip->lock in order to
5034 // read or write that inode's ip->valid, ip->size, ip->type, &c.
5035 
5036 struct {
5037   struct spinlock lock;
5038   struct inode inode[NINODE];
5039 } itable;
5040 
5041 void
5042 iinit()
5043 {
5044   int i = 0;
5045 
5046   initlock(&itable.lock, "itable");
5047   for(i = 0; i < NINODE; i++) {
5048     initsleeplock(&itable.inode[i].lock, "inode");
5049   }
5050 }
5051 
5052 static struct inode* iget(uint dev, uint inum);
5053 
5054 // Allocate an inode on device dev.
5055 // Mark it as allocated by  giving it type type.
5056 // Returns an unlocked but allocated and referenced inode,
5057 // or NULL if there is no free inode.
5058 struct inode*
5059 ialloc(uint dev, short type)
5060 {
5061   int inum;
5062   struct buf *bp;
5063   struct dinode *dip;
5064 
5065   for(inum = 1; inum < sb.ninodes; inum++){
5066     bp = bread(dev, IBLOCK(inum, sb));
5067     dip = (struct dinode*)bp->data + inum%IPB;
5068     if(dip->type == 0){  // a free inode
5069       memset(dip, 0, sizeof(*dip));
5070       dip->type = type;
5071       log_write(bp);   // mark it allocated on the disk
5072       brelse(bp);
5073       return iget(dev, inum);
5074     }
5075     brelse(bp);
5076   }
5077   printf("ialloc: no inodes\n");
5078   return 0;
5079 }
5080 
5081 // Copy a modified in-memory inode to disk.
5082 // Must be called after every change to an ip->xxx field
5083 // that lives on disk.
5084 // Caller must hold ip->lock.
5085 void
5086 iupdate(struct inode *ip)
5087 {
5088   struct buf *bp;
5089   struct dinode *dip;
5090 
5091   bp = bread(ip->dev, IBLOCK(ip->inum, sb));
5092   dip = (struct dinode*)bp->data + ip->inum%IPB;
5093   dip->type = ip->type;
5094   dip->major = ip->major;
5095   dip->minor = ip->minor;
5096   dip->nlink = ip->nlink;
5097   dip->size = ip->size;
5098   memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
5099   log_write(bp);
5100   brelse(bp);
5101 }
5102 
5103 // Find the inode with number inum on device dev
5104 // and return the in-memory copy. Does not lock
5105 // the inode and does not read it from disk.
5106 static struct inode*
5107 iget(uint dev, uint inum)
5108 {
5109   struct inode *ip, *empty;
5110 
5111   acquire(&itable.lock);
5112 
5113   // Is the inode already in the table?
5114   empty = 0;
5115   for(ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++){
5116     if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
5117       ip->ref++;
5118       release(&itable.lock);
5119       return ip;
5120     }
5121     if(empty == 0 && ip->ref == 0)    // Remember empty slot.
5122       empty = ip;
5123   }
5124 
5125   // Recycle an inode entry.
5126   if(empty == 0)
5127     panic("iget: no inodes");
5128 
5129   ip = empty;
5130   ip->dev = dev;
5131   ip->inum = inum;
5132   ip->ref = 1;
5133   ip->valid = 0;
5134   release(&itable.lock);
5135 
5136   return ip;
5137 }
5138 
5139 // Increment reference count for ip.
5140 // Returns ip to enable ip = idup(ip1) idiom.
5141 struct inode*
5142 idup(struct inode *ip)
5143 {
5144   acquire(&itable.lock);
5145   ip->ref++;
5146   release(&itable.lock);
5147   return ip;
5148 }
5149 
5150 // Lock the given inode.
5151 // Reads the inode from disk if necessary.
5152 void
5153 ilock(struct inode *ip)
5154 {
5155   struct buf *bp;
5156   struct dinode *dip;
5157 
5158   if(ip == 0 || ip->ref < 1)
5159     panic("ilock");
5160 
5161   acquiresleep(&ip->lock);
5162 
5163   if(ip->valid == 0){
5164     bp = bread(ip->dev, IBLOCK(ip->inum, sb));
5165     dip = (struct dinode*)bp->data + ip->inum%IPB;
5166     ip->type = dip->type;
5167     ip->major = dip->major;
5168     ip->minor = dip->minor;
5169     ip->nlink = dip->nlink;
5170     ip->size = dip->size;
5171     memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
5172     brelse(bp);
5173     ip->valid = 1;
5174     if(ip->type == 0)
5175       panic("ilock: no type");
5176   }
5177 }
5178 
5179 // Unlock the given inode.
5180 void
5181 iunlock(struct inode *ip)
5182 {
5183   if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
5184     panic("iunlock");
5185 
5186   releasesleep(&ip->lock);
5187 }
5188 
5189 
5190 
5191 
5192 
5193 
5194 
5195 
5196 
5197 
5198 
5199 
5200 // Drop a reference to an in-memory inode.
5201 // If that was the last reference, the inode table entry can
5202 // be recycled.
5203 // If that was the last reference and the inode has no links
5204 // to it, free the inode (and its content) on disk.
5205 // All calls to iput() must be inside a transaction in
5206 // case it has to free the inode.
5207 void
5208 iput(struct inode *ip)
5209 {
5210   acquire(&itable.lock);
5211 
5212   if(ip->ref == 1 && ip->valid && ip->nlink == 0){
5213     // inode has no links and no other references: truncate and free.
5214 
5215     // ip->ref == 1 means no other process can have ip locked,
5216     // so this acquiresleep() won't block (or deadlock).
5217     acquiresleep(&ip->lock);
5218 
5219     release(&itable.lock);
5220 
5221     itrunc(ip);
5222     ip->type = 0;
5223     iupdate(ip);
5224     ip->valid = 0;
5225 
5226     releasesleep(&ip->lock);
5227 
5228     acquire(&itable.lock);
5229   }
5230 
5231   ip->ref--;
5232   release(&itable.lock);
5233 }
5234 
5235 // Common idiom: unlock, then put.
5236 void
5237 iunlockput(struct inode *ip)
5238 {
5239   iunlock(ip);
5240   iput(ip);
5241 }
5242 
5243 
5244 
5245 
5246 
5247 
5248 
5249 
5250 void
5251 ireclaim(int dev)
5252 {
5253   for (int inum = 1; inum < sb.ninodes; inum++) {
5254     struct inode *ip = 0;
5255     struct buf *bp = bread(dev, IBLOCK(inum, sb));
5256     struct dinode *dip = (struct dinode *)bp->data + inum % IPB;
5257     if (dip->type != 0 && dip->nlink == 0) {  // is an orphaned inode
5258       printf("ireclaim: orphaned inode %d\n", inum);
5259       ip = iget(dev, inum);
5260     }
5261     brelse(bp);
5262     if (ip) {
5263       begin_op();
5264       ilock(ip);
5265       iunlock(ip);
5266       iput(ip);
5267       end_op();
5268     }
5269   }
5270 }
5271 
5272 // Inode content
5273 //
5274 // The content (data) associated with each inode is stored
5275 // in blocks on the disk. The first NDIRECT block numbers
5276 // are listed in ip->addrs[].  The next NINDIRECT blocks are
5277 // listed in block ip->addrs[NDIRECT].
5278 
5279 // Return the disk block address of the nth block in inode ip.
5280 // If there is no such block, bmap allocates one.
5281 // returns 0 if out of disk space.
5282 static uint
5283 bmap(struct inode *ip, uint bn)
5284 {
5285   uint addr, *a;
5286   struct buf *bp;
5287 
5288   if(bn < NDIRECT){
5289     if((addr = ip->addrs[bn]) == 0){
5290       addr = balloc(ip->dev);
5291       if(addr == 0)
5292         return 0;
5293       ip->addrs[bn] = addr;
5294     }
5295     return addr;
5296   }
5297   bn -= NDIRECT;
5298 
5299 
5300   if(bn < NINDIRECT){
5301     // Load indirect block, allocating if necessary.
5302     if((addr = ip->addrs[NDIRECT]) == 0){
5303       addr = balloc(ip->dev);
5304       if(addr == 0)
5305         return 0;
5306       ip->addrs[NDIRECT] = addr;
5307     }
5308     bp = bread(ip->dev, addr);
5309     a = (uint*)bp->data;
5310     if((addr = a[bn]) == 0){
5311       addr = balloc(ip->dev);
5312       if(addr){
5313         a[bn] = addr;
5314         log_write(bp);
5315       }
5316     }
5317     brelse(bp);
5318     return addr;
5319   }
5320 
5321   panic("bmap: out of range");
5322 }
5323 
5324 // Truncate inode (discard contents).
5325 // Caller must hold ip->lock.
5326 void
5327 itrunc(struct inode *ip)
5328 {
5329   int i, j;
5330   struct buf *bp;
5331   uint *a;
5332 
5333   for(i = 0; i < NDIRECT; i++){
5334     if(ip->addrs[i]){
5335       bfree(ip->dev, ip->addrs[i]);
5336       ip->addrs[i] = 0;
5337     }
5338   }
5339 
5340   if(ip->addrs[NDIRECT]){
5341     bp = bread(ip->dev, ip->addrs[NDIRECT]);
5342     a = (uint*)bp->data;
5343     for(j = 0; j < NINDIRECT; j++){
5344       if(a[j])
5345         bfree(ip->dev, a[j]);
5346     }
5347     brelse(bp);
5348     bfree(ip->dev, ip->addrs[NDIRECT]);
5349     ip->addrs[NDIRECT] = 0;
5350   }
5351 
5352   ip->size = 0;
5353   iupdate(ip);
5354 }
5355 
5356 // Copy stat information from inode.
5357 // Caller must hold ip->lock.
5358 void
5359 stati(struct inode *ip, struct stat *st)
5360 {
5361   st->dev = ip->dev;
5362   st->ino = ip->inum;
5363   st->type = ip->type;
5364   st->nlink = ip->nlink;
5365   st->size = ip->size;
5366 }
5367 
5368 // Read data from inode.
5369 // Caller must hold ip->lock.
5370 // If user_dst==1, then dst is a user virtual address;
5371 // otherwise, dst is a kernel address.
5372 int
5373 readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
5374 {
5375   uint tot, m;
5376   struct buf *bp;
5377 
5378   if(off > ip->size || off + n < off)
5379     return 0;
5380   if(off + n > ip->size)
5381     n = ip->size - off;
5382 
5383   for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
5384     uint addr = bmap(ip, off/BSIZE);
5385     if(addr == 0)
5386       break;
5387     bp = bread(ip->dev, addr);
5388     m = min(n - tot, BSIZE - off%BSIZE);
5389     if(either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
5390       brelse(bp);
5391       tot = -1;
5392       break;
5393     }
5394     brelse(bp);
5395   }
5396   return tot;
5397 }
5398 
5399 
5400 // Write data to inode.
5401 // Caller must hold ip->lock.
5402 // If user_src==1, then src is a user virtual address;
5403 // otherwise, src is a kernel address.
5404 // Returns the number of bytes successfully written.
5405 // If the return value is less than the requested n,
5406 // there was an error of some kind.
5407 int
5408 writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
5409 {
5410   uint tot, m;
5411   struct buf *bp;
5412 
5413   if(off > ip->size || off + n < off)
5414     return -1;
5415   if(off + n > MAXFILE*BSIZE)
5416     return -1;
5417 
5418   for(tot=0; tot<n; tot+=m, off+=m, src+=m){
5419     uint addr = bmap(ip, off/BSIZE);
5420     if(addr == 0)
5421       break;
5422     bp = bread(ip->dev, addr);
5423     m = min(n - tot, BSIZE - off%BSIZE);
5424     if(either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
5425       brelse(bp);
5426       break;
5427     }
5428     log_write(bp);
5429     brelse(bp);
5430   }
5431 
5432   if(off > ip->size)
5433     ip->size = off;
5434 
5435   // write the i-node back to disk even if the size didn't change
5436   // because the loop above might have called bmap() and added a new
5437   // block to ip->addrs[].
5438   iupdate(ip);
5439 
5440   return tot;
5441 }
5442 
5443 // Directories
5444 
5445 int
5446 namecmp(const char *s, const char *t)
5447 {
5448   return strncmp(s, t, DIRSIZ);
5449 }
5450 // Look for a directory entry in a directory.
5451 // If found, set *poff to byte offset of entry.
5452 struct inode*
5453 dirlookup(struct inode *dp, char *name, uint *poff)
5454 {
5455   uint off, inum;
5456   struct dirent de;
5457 
5458   if(dp->type != T_DIR)
5459     panic("dirlookup not DIR");
5460 
5461   for(off = 0; off < dp->size; off += sizeof(de)){
5462     if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
5463       panic("dirlookup read");
5464     if(de.inum == 0)
5465       continue;
5466     if(namecmp(name, de.name) == 0){
5467       // entry matches path element
5468       if(poff)
5469         *poff = off;
5470       inum = de.inum;
5471       return iget(dp->dev, inum);
5472     }
5473   }
5474 
5475   return 0;
5476 }
5477 
5478 // Write a new directory entry (name, inum) into the directory dp.
5479 // Returns 0 on success, -1 on failure (e.g. out of disk blocks).
5480 int
5481 dirlink(struct inode *dp, char *name, uint inum)
5482 {
5483   int off;
5484   struct dirent de;
5485   struct inode *ip;
5486 
5487   // Check that name is not present.
5488   if((ip = dirlookup(dp, name, 0)) != 0){
5489     iput(ip);
5490     return -1;
5491   }
5492 
5493   // Look for an empty dirent.
5494   for(off = 0; off < dp->size; off += sizeof(de)){
5495     if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
5496       panic("dirlink read");
5497     if(de.inum == 0)
5498       break;
5499   }
5500   strncpy(de.name, name, DIRSIZ);
5501   de.inum = inum;
5502   if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
5503     return -1;
5504 
5505   return 0;
5506 }
5507 
5508 // Paths
5509 
5510 // Copy the next path element from path into name.
5511 // Return a pointer to the element following the copied one.
5512 // The returned path has no leading slashes,
5513 // so the caller can check *path=='\0' to see if the name is the last one.
5514 // If no name to remove, return 0.
5515 //
5516 // Examples:
5517 //   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
5518 //   skipelem("///a//bb", name) = "bb", setting name = "a"
5519 //   skipelem("a", name) = "", setting name = "a"
5520 //   skipelem("", name) = skipelem("////", name) = 0
5521 //
5522 static char*
5523 skipelem(char *path, char *name)
5524 {
5525   char *s;
5526   int len;
5527 
5528   while(*path == '/')
5529     path++;
5530   if(*path == 0)
5531     return 0;
5532   s = path;
5533   while(*path != '/' && *path != 0)
5534     path++;
5535   len = path - s;
5536   if(len >= DIRSIZ)
5537     memmove(name, s, DIRSIZ);
5538   else {
5539     memmove(name, s, len);
5540     name[len] = 0;
5541   }
5542   while(*path == '/')
5543     path++;
5544   return path;
5545 }
5546 
5547 
5548 
5549 
5550 // Look up and return the inode for a path name.
5551 // If parent != 0, return the inode for the parent and copy the final
5552 // path element into name, which must have room for DIRSIZ bytes.
5553 // Must be called inside a transaction since it calls iput().
5554 static struct inode*
5555 namex(char *path, int nameiparent, char *name)
5556 {
5557   struct inode *ip, *next;
5558 
5559   if(*path == '/')
5560     ip = iget(ROOTDEV, ROOTINO);
5561   else
5562     ip = idup(myproc()->cwd);
5563 
5564   while((path = skipelem(path, name)) != 0){
5565     ilock(ip);
5566     if(ip->type != T_DIR){
5567       iunlockput(ip);
5568       return 0;
5569     }
5570     if(nameiparent && *path == '\0'){
5571       // Stop one level early.
5572       iunlock(ip);
5573       return ip;
5574     }
5575     if((next = dirlookup(ip, name, 0)) == 0){
5576       iunlockput(ip);
5577       return 0;
5578     }
5579     iunlockput(ip);
5580     ip = next;
5581   }
5582   if(nameiparent){
5583     iput(ip);
5584     return 0;
5585   }
5586   return ip;
5587 }
5588 
5589 struct inode*
5590 namei(char *path)
5591 {
5592   char name[DIRSIZ];
5593   return namex(path, 0, name);
5594 }
5595 
5596 
5597 
5598 
5599 
5600 struct inode*
5601 nameiparent(char *path, char *name)
5602 {
5603   return namex(path, 1, name);
5604 }
5605 
5606 
5607 
5608 
5609 
5610 
5611 
5612 
5613 
5614 
5615 
5616 
5617 
5618 
5619 
5620 
5621 
5622 
5623 
5624 
5625 
5626 
5627 
5628 
5629 
5630 
5631 
5632 
5633 
5634 
5635 
5636 
5637 
5638 
5639 
5640 
5641 
5642 
5643 
5644 
5645 
5646 
5647 
5648 
5649 

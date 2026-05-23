4200 struct file {
4201   enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
4202   int ref; // reference count
4203   char readable;
4204   char writable;
4205   struct pipe *pipe; // FD_PIPE
4206   struct inode *ip;  // FD_INODE and FD_DEVICE
4207   uint off;          // FD_INODE
4208   short major;       // FD_DEVICE
4209 };
4210 
4211 #define major(dev)  ((dev) >> 16 & 0xFFFF)
4212 #define minor(dev)  ((dev) & 0xFFFF)
4213 #define	mkdev(m,n)  ((uint)((m)<<16| (n)))
4214 
4215 // in-memory copy of an inode
4216 struct inode {
4217   uint dev;           // Device number
4218   uint inum;          // Inode number
4219   int ref;            // Reference count
4220   struct sleeplock lock; // protects everything below here
4221   int valid;          // inode has been read from disk?
4222 
4223   short type;         // copy of disk inode
4224   short major;
4225   short minor;
4226   short nlink;
4227   uint size;
4228   uint addrs[NDIRECT+1];
4229 };
4230 
4231 // map major device number to device functions.
4232 struct devsw {
4233   int (*read)(int, uint64, int);
4234   int (*write)(int, uint64, int);
4235 };
4236 
4237 extern struct devsw devsw[];
4238 
4239 #define CONSOLE 1
4240 
4241 
4242 
4243 
4244 
4245 
4246 
4247 
4248 
4249 

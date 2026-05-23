5850 //
5851 // File-system system calls.
5852 // Mostly argument checking, since we don't trust
5853 // user code, and calls into file.c and fs.c.
5854 //
5855 
5856 #include "types.h"
5857 #include "riscv.h"
5858 #include "defs.h"
5859 #include "param.h"
5860 #include "stat.h"
5861 #include "spinlock.h"
5862 #include "proc.h"
5863 #include "fs.h"
5864 #include "sleeplock.h"
5865 #include "file.h"
5866 #include "fcntl.h"
5867 
5868 // Fetch the nth word-sized system call argument as a file descriptor
5869 // and return both the descriptor and the corresponding struct file.
5870 static int
5871 argfd(int n, int *pfd, struct file **pf)
5872 {
5873   int fd;
5874   struct file *f;
5875 
5876   argint(n, &fd);
5877   if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
5878     return -1;
5879   if(pfd)
5880     *pfd = fd;
5881   if(pf)
5882     *pf = f;
5883   return 0;
5884 }
5885 
5886 
5887 
5888 
5889 
5890 
5891 
5892 
5893 
5894 
5895 
5896 
5897 
5898 
5899 
5900 // Allocate a file descriptor for the given file.
5901 // Takes over file reference from caller on success.
5902 static int
5903 fdalloc(struct file *f)
5904 {
5905   int fd;
5906   struct proc *p = myproc();
5907 
5908   for(fd = 0; fd < NOFILE; fd++){
5909     if(p->ofile[fd] == 0){
5910       p->ofile[fd] = f;
5911       return fd;
5912     }
5913   }
5914   return -1;
5915 }
5916 
5917 uint64
5918 sys_dup(void)
5919 {
5920   struct file *f;
5921   int fd;
5922 
5923   if(argfd(0, 0, &f) < 0)
5924     return -1;
5925   if((fd=fdalloc(f)) < 0)
5926     return -1;
5927   filedup(f);
5928   return fd;
5929 }
5930 
5931 uint64
5932 sys_read(void)
5933 {
5934   struct file *f;
5935   int n;
5936   uint64 p;
5937 
5938   argaddr(1, &p);
5939   argint(2, &n);
5940   if(argfd(0, 0, &f) < 0)
5941     return -1;
5942   return fileread(f, p, n);
5943 }
5944 
5945 
5946 
5947 
5948 
5949 
5950 uint64
5951 sys_write(void)
5952 {
5953   struct file *f;
5954   int n;
5955   uint64 p;
5956 
5957   argaddr(1, &p);
5958   argint(2, &n);
5959   if(argfd(0, 0, &f) < 0)
5960     return -1;
5961 
5962   return filewrite(f, p, n);
5963 }
5964 
5965 uint64
5966 sys_close(void)
5967 {
5968   int fd;
5969   struct file *f;
5970 
5971   if(argfd(0, &fd, &f) < 0)
5972     return -1;
5973   myproc()->ofile[fd] = 0;
5974   fileclose(f);
5975   return 0;
5976 }
5977 
5978 uint64
5979 sys_fstat(void)
5980 {
5981   struct file *f;
5982   uint64 st; // user pointer to struct stat
5983 
5984   argaddr(1, &st);
5985   if(argfd(0, 0, &f) < 0)
5986     return -1;
5987   return filestat(f, st);
5988 }
5989 
5990 
5991 
5992 
5993 
5994 
5995 
5996 
5997 
5998 
5999 
6000 // Create the path new as a link to the same inode as old.
6001 uint64
6002 sys_link(void)
6003 {
6004   char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
6005   struct inode *dp, *ip;
6006 
6007   if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
6008     return -1;
6009 
6010   begin_op();
6011   if((ip = namei(old)) == 0){
6012     end_op();
6013     return -1;
6014   }
6015 
6016   ilock(ip);
6017   if(ip->type == T_DIR){
6018     iunlockput(ip);
6019     end_op();
6020     return -1;
6021   }
6022 
6023   ip->nlink++;
6024   iupdate(ip);
6025   iunlock(ip);
6026 
6027   if((dp = nameiparent(new, name)) == 0)
6028     goto bad;
6029   ilock(dp);
6030   if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
6031     iunlockput(dp);
6032     goto bad;
6033   }
6034   iunlockput(dp);
6035   iput(ip);
6036 
6037   end_op();
6038 
6039   return 0;
6040 
6041 bad:
6042   ilock(ip);
6043   ip->nlink--;
6044   iupdate(ip);
6045   iunlockput(ip);
6046   end_op();
6047   return -1;
6048 }
6049 
6050 // Is the directory dp empty except for "." and ".." ?
6051 static int
6052 isdirempty(struct inode *dp)
6053 {
6054   int off;
6055   struct dirent de;
6056 
6057   for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
6058     if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
6059       panic("isdirempty: readi");
6060     if(de.inum != 0)
6061       return 0;
6062   }
6063   return 1;
6064 }
6065 
6066 uint64
6067 sys_unlink(void)
6068 {
6069   struct inode *ip, *dp;
6070   struct dirent de;
6071   char name[DIRSIZ], path[MAXPATH];
6072   uint off;
6073 
6074   if(argstr(0, path, MAXPATH) < 0)
6075     return -1;
6076 
6077   begin_op();
6078   if((dp = nameiparent(path, name)) == 0){
6079     end_op();
6080     return -1;
6081   }
6082 
6083   ilock(dp);
6084 
6085   // Cannot unlink "." or "..".
6086   if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
6087     goto bad;
6088 
6089   if((ip = dirlookup(dp, name, &off)) == 0)
6090     goto bad;
6091   ilock(ip);
6092 
6093   if(ip->nlink < 1)
6094     panic("unlink: nlink < 1");
6095   if(ip->type == T_DIR && !isdirempty(ip)){
6096     iunlockput(ip);
6097     goto bad;
6098   }
6099 
6100   memset(&de, 0, sizeof(de));
6101   if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
6102     panic("unlink: writei");
6103   if(ip->type == T_DIR){
6104     dp->nlink--;
6105     iupdate(dp);
6106   }
6107   iunlockput(dp);
6108 
6109   ip->nlink--;
6110   iupdate(ip);
6111   iunlockput(ip);
6112 
6113   end_op();
6114 
6115   return 0;
6116 
6117 bad:
6118   iunlockput(dp);
6119   end_op();
6120   return -1;
6121 }
6122 
6123 static struct inode*
6124 create(char *path, short type, short major, short minor)
6125 {
6126   struct inode *ip, *dp;
6127   char name[DIRSIZ];
6128 
6129   if((dp = nameiparent(path, name)) == 0)
6130     return 0;
6131 
6132   ilock(dp);
6133 
6134   if((ip = dirlookup(dp, name, 0)) != 0){
6135     iunlockput(dp);
6136     ilock(ip);
6137     if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
6138       return ip;
6139     iunlockput(ip);
6140     return 0;
6141   }
6142 
6143   if((ip = ialloc(dp->dev, type)) == 0){
6144     iunlockput(dp);
6145     return 0;
6146   }
6147 
6148 
6149 
6150   ilock(ip);
6151   ip->major = major;
6152   ip->minor = minor;
6153   ip->nlink = 1;
6154   iupdate(ip);
6155 
6156   if(type == T_DIR){  // Create . and .. entries.
6157     // No ip->nlink++ for ".": avoid cyclic ref count.
6158     if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
6159       goto fail;
6160   }
6161 
6162   if(dirlink(dp, name, ip->inum) < 0)
6163     goto fail;
6164 
6165   if(type == T_DIR){
6166     // now that success is guaranteed:
6167     dp->nlink++;  // for ".."
6168     iupdate(dp);
6169   }
6170 
6171   iunlockput(dp);
6172 
6173   return ip;
6174 
6175  fail:
6176   // something went wrong. de-allocate ip.
6177   ip->nlink = 0;
6178   iupdate(ip);
6179   iunlockput(ip);
6180   iunlockput(dp);
6181   return 0;
6182 }
6183 
6184 uint64
6185 sys_open(void)
6186 {
6187   char path[MAXPATH];
6188   int fd, omode;
6189   struct file *f;
6190   struct inode *ip;
6191   int n;
6192 
6193   argint(1, &omode);
6194   if((n = argstr(0, path, MAXPATH)) < 0)
6195     return -1;
6196 
6197   begin_op();
6198 
6199 
6200   if(omode & O_CREATE){
6201     ip = create(path, T_FILE, 0, 0);
6202     if(ip == 0){
6203       end_op();
6204       return -1;
6205     }
6206   } else {
6207     if((ip = namei(path)) == 0){
6208       end_op();
6209       return -1;
6210     }
6211     ilock(ip);
6212     if(ip->type == T_DIR && omode != O_RDONLY){
6213       iunlockput(ip);
6214       end_op();
6215       return -1;
6216     }
6217   }
6218 
6219   if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
6220     iunlockput(ip);
6221     end_op();
6222     return -1;
6223   }
6224 
6225   if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
6226     if(f)
6227       fileclose(f);
6228     iunlockput(ip);
6229     end_op();
6230     return -1;
6231   }
6232 
6233   if(ip->type == T_DEVICE){
6234     f->type = FD_DEVICE;
6235     f->major = ip->major;
6236   } else {
6237     f->type = FD_INODE;
6238     f->off = 0;
6239   }
6240   f->ip = ip;
6241   f->readable = !(omode & O_WRONLY);
6242   f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
6243 
6244   if((omode & O_TRUNC) && ip->type == T_FILE){
6245     itrunc(ip);
6246   }
6247 
6248   iunlock(ip);
6249   end_op();
6250   return fd;
6251 }
6252 
6253 uint64
6254 sys_mkdir(void)
6255 {
6256   char path[MAXPATH];
6257   struct inode *ip;
6258 
6259   begin_op();
6260   if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
6261     end_op();
6262     return -1;
6263   }
6264   iunlockput(ip);
6265   end_op();
6266   return 0;
6267 }
6268 
6269 uint64
6270 sys_mknod(void)
6271 {
6272   struct inode *ip;
6273   char path[MAXPATH];
6274   int major, minor;
6275 
6276   begin_op();
6277   argint(1, &major);
6278   argint(2, &minor);
6279   if((argstr(0, path, MAXPATH)) < 0 ||
6280      (ip = create(path, T_DEVICE, major, minor)) == 0){
6281     end_op();
6282     return -1;
6283   }
6284   iunlockput(ip);
6285   end_op();
6286   return 0;
6287 }
6288 
6289 
6290 
6291 
6292 
6293 
6294 
6295 
6296 
6297 
6298 
6299 
6300 uint64
6301 sys_chdir(void)
6302 {
6303   char path[MAXPATH];
6304   struct inode *ip;
6305   struct proc *p = myproc();
6306 
6307   begin_op();
6308   if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0){
6309     end_op();
6310     return -1;
6311   }
6312   ilock(ip);
6313   if(ip->type != T_DIR){
6314     iunlockput(ip);
6315     end_op();
6316     return -1;
6317   }
6318   iunlock(ip);
6319   iput(p->cwd);
6320   end_op();
6321   p->cwd = ip;
6322   return 0;
6323 }
6324 
6325 uint64
6326 sys_exec(void)
6327 {
6328   char path[MAXPATH], *argv[MAXARG];
6329   int i;
6330   uint64 uargv, uarg;
6331 
6332   argaddr(1, &uargv);
6333   if(argstr(0, path, MAXPATH) < 0) {
6334     return -1;
6335   }
6336   memset(argv, 0, sizeof(argv));
6337   for(i=0;; i++){
6338     if(i >= NELEM(argv)){
6339       goto bad;
6340     }
6341     if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
6342       goto bad;
6343     }
6344     if(uarg == 0){
6345       argv[i] = 0;
6346       break;
6347     }
6348     argv[i] = kalloc();
6349     if(argv[i] == 0)
6350       goto bad;
6351     if(fetchstr(uarg, argv[i], PGSIZE) < 0)
6352       goto bad;
6353   }
6354 
6355   int ret = kexec(path, argv);
6356 
6357   for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
6358     kfree(argv[i]);
6359 
6360   return ret;
6361 
6362  bad:
6363   for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
6364     kfree(argv[i]);
6365   return -1;
6366 }
6367 
6368 uint64
6369 sys_pipe(void)
6370 {
6371   uint64 fdarray; // user pointer to array of two integers
6372   struct file *rf, *wf;
6373   int fd0, fd1;
6374   struct proc *p = myproc();
6375 
6376   argaddr(0, &fdarray);
6377   if(pipealloc(&rf, &wf) < 0)
6378     return -1;
6379   fd0 = -1;
6380   if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
6381     if(fd0 >= 0)
6382       p->ofile[fd0] = 0;
6383     fileclose(rf);
6384     fileclose(wf);
6385     return -1;
6386   }
6387   if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
6388      copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
6389     p->ofile[fd0] = 0;
6390     p->ofile[fd1] = 0;
6391     fileclose(rf);
6392     fileclose(wf);
6393     return -1;
6394   }
6395   return 0;
6396 }
6397 
6398 
6399 

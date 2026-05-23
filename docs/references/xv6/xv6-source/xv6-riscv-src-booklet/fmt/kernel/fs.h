4100 // On-disk file system format.
4101 // Both the kernel and user programs use this header file.
4102 
4103 
4104 #define ROOTINO  1   // root i-number
4105 #define BSIZE 1024  // block size
4106 
4107 // Disk layout:
4108 // [ boot block | super block | log | inode blocks |
4109 //                                          free bit map | data blocks]
4110 //
4111 // mkfs computes the super block and builds an initial file system. The
4112 // super block describes the disk layout:
4113 struct superblock {
4114   uint magic;        // Must be FSMAGIC
4115   uint size;         // Size of file system image (blocks)
4116   uint nblocks;      // Number of data blocks
4117   uint ninodes;      // Number of inodes.
4118   uint nlog;         // Number of log blocks
4119   uint logstart;     // Block number of first log block
4120   uint inodestart;   // Block number of first inode block
4121   uint bmapstart;    // Block number of first free map block
4122 };
4123 
4124 #define FSMAGIC 0x10203040
4125 
4126 #define NDIRECT 12
4127 #define NINDIRECT (BSIZE / sizeof(uint))
4128 #define MAXFILE (NDIRECT + NINDIRECT)
4129 
4130 // On-disk inode structure
4131 struct dinode {
4132   short type;           // File type
4133   short major;          // Major device number (T_DEVICE only)
4134   short minor;          // Minor device number (T_DEVICE only)
4135   short nlink;          // Number of links to inode in file system
4136   uint size;            // Size of file (bytes)
4137   uint addrs[NDIRECT+1];   // Data block addresses
4138 };
4139 
4140 
4141 
4142 
4143 
4144 
4145 
4146 
4147 
4148 
4149 
4150 // Inodes per block.
4151 #define IPB           (BSIZE / sizeof(struct dinode))
4152 
4153 // Block containing inode i
4154 #define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)
4155 
4156 // Bitmap bits per block
4157 #define BPB           (BSIZE*8)
4158 
4159 // Block of free map containing bit for block b
4160 #define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)
4161 
4162 // Directory is a file containing a sequence of dirent structures.
4163 #define DIRSIZ 14
4164 
4165 // The name field may have DIRSIZ characters and not end in a NUL
4166 // character.
4167 struct dirent {
4168   ushort inum;
4169   char name[DIRSIZ] __attribute__((nonstring));
4170 };
4171 
4172 
4173 
4174 
4175 
4176 
4177 
4178 
4179 
4180 
4181 
4182 
4183 
4184 
4185 
4186 
4187 
4188 
4189 
4190 
4191 
4192 
4193 
4194 
4195 
4196 
4197 
4198 
4199 

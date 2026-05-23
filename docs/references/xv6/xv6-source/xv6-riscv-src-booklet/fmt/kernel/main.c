1150 #include "types.h"
1151 #include "param.h"
1152 #include "memlayout.h"
1153 #include "riscv.h"
1154 #include "defs.h"
1155 
1156 volatile static int started = 0;
1157 
1158 // start() jumps here in supervisor mode on all CPUs.
1159 void
1160 main()
1161 {
1162   if(cpuid() == 0){
1163     consoleinit();
1164     printfinit();
1165     printf("\n");
1166     printf("xv6 kernel is booting\n");
1167     printf("\n");
1168     kinit();         // physical page allocator
1169     kvminit();       // create kernel page table
1170     kvminithart();   // turn on paging
1171     procinit();      // process table
1172     trapinit();      // trap vectors
1173     trapinithart();  // install kernel trap vector
1174     plicinit();      // set up interrupt controller
1175     plicinithart();  // ask PLIC for device interrupts
1176     binit();         // buffer cache
1177     iinit();         // inode table
1178     fileinit();      // file table
1179     virtio_disk_init(); // emulated hard disk
1180     userinit();      // first user process
1181     __sync_synchronize();
1182     started = 1;
1183   } else {
1184     while(started == 0)
1185       ;
1186     __sync_synchronize();
1187     printf("hart %d starting\n", cpuid());
1188     kvminithart();    // turn on paging
1189     trapinithart();   // install kernel trap vector
1190     plicinithart();   // ask PLIC for device interrupts
1191   }
1192 
1193   scheduler();
1194 }
1195 
1196 
1197 
1198 
1199 

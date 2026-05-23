1250 // Mutual exclusion spin locks.
1251 
1252 #include "types.h"
1253 #include "param.h"
1254 #include "memlayout.h"
1255 #include "spinlock.h"
1256 #include "riscv.h"
1257 #include "proc.h"
1258 #include "defs.h"
1259 
1260 void
1261 initlock(struct spinlock *lk, char *name)
1262 {
1263   lk->name = name;
1264   lk->locked = 0;
1265   lk->cpu = 0;
1266 }
1267 
1268 // Acquire the lock.
1269 // Loops (spins) until the lock is acquired.
1270 void
1271 acquire(struct spinlock *lk)
1272 {
1273   push_off(); // disable interrupts to avoid deadlock.
1274   if(holding(lk))
1275     panic("acquire");
1276 
1277   // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
1278   //   a5 = 1
1279   //   s1 = &lk->locked
1280   //   amoswap.w.aq a5, a5, (s1)
1281   while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
1282     ;
1283 
1284   // Tell the C compiler and the processor to not move loads or stores
1285   // past this point, to ensure that the critical section's memory
1286   // references happen strictly after the lock is acquired.
1287   // On RISC-V, this emits a fence instruction.
1288   __sync_synchronize();
1289 
1290   // Record info about lock acquisition for holding() and debugging.
1291   lk->cpu = mycpu();
1292 }
1293 
1294 
1295 
1296 
1297 
1298 
1299 
1300 // Release the lock.
1301 void
1302 release(struct spinlock *lk)
1303 {
1304   if(!holding(lk))
1305     panic("release");
1306 
1307   lk->cpu = 0;
1308 
1309   // Tell the C compiler and the CPU to not move loads or stores
1310   // past this point, to ensure that all the stores in the critical
1311   // section are visible to other CPUs before the lock is released,
1312   // and that loads in the critical section occur strictly before
1313   // the lock is released.
1314   // On RISC-V, this emits a fence instruction.
1315   __sync_synchronize();
1316 
1317   // Release the lock, equivalent to lk->locked = 0.
1318   // This code doesn't use a C assignment, since the C standard
1319   // implies that an assignment might be implemented with
1320   // multiple store instructions.
1321   // On RISC-V, sync_lock_release turns into an atomic swap:
1322   //   s1 = &lk->locked
1323   //   amoswap.w zero, zero, (s1)
1324   __sync_lock_release(&lk->locked);
1325 
1326   pop_off();
1327 }
1328 
1329 // Check whether this cpu is holding the lock.
1330 // Interrupts must be off.
1331 int
1332 holding(struct spinlock *lk)
1333 {
1334   int r;
1335   r = (lk->locked && lk->cpu == mycpu());
1336   return r;
1337 }
1338 
1339 
1340 
1341 
1342 
1343 
1344 
1345 
1346 
1347 
1348 
1349 
1350 // push_off/pop_off are like intr_off()/intr_on() except that they are matched:
1351 // it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
1352 // are initially off, then push_off, pop_off leaves them off.
1353 
1354 void
1355 push_off(void)
1356 {
1357   int old = intr_get();
1358 
1359   // disable interrupts to prevent an involuntary context
1360   // switch while using mycpu().
1361   intr_off();
1362 
1363   if(mycpu()->noff == 0)
1364     mycpu()->intena = old;
1365   mycpu()->noff += 1;
1366 }
1367 
1368 void
1369 pop_off(void)
1370 {
1371   struct cpu *c = mycpu();
1372   if(intr_get())
1373     panic("pop_off - interruptible");
1374   if(c->noff < 1)
1375     panic("pop_off");
1376   c->noff -= 1;
1377   if(c->noff == 0 && c->intena)
1378     intr_on();
1379 }
1380 
1381 
1382 
1383 
1384 
1385 
1386 
1387 
1388 
1389 
1390 
1391 
1392 
1393 
1394 
1395 
1396 
1397 
1398 
1399 

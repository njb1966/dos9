4250 // Buffer cache.
4251 //
4252 // The buffer cache is a linked list of buf structures holding
4253 // cached copies of disk block contents.  Caching disk blocks
4254 // in memory reduces the number of disk reads and also provides
4255 // a synchronization point for disk blocks used by multiple processes.
4256 //
4257 // Interface:
4258 // * To get a buffer for a particular disk block, call bread.
4259 // * After changing buffer data, call bwrite to write it to disk.
4260 // * When done with the buffer, call brelse.
4261 // * Do not use the buffer after calling brelse.
4262 // * Only one process at a time can use a buffer,
4263 //     so do not keep them longer than necessary.
4264 
4265 
4266 #include "types.h"
4267 #include "param.h"
4268 #include "spinlock.h"
4269 #include "sleeplock.h"
4270 #include "riscv.h"
4271 #include "defs.h"
4272 #include "fs.h"
4273 #include "buf.h"
4274 
4275 struct {
4276   struct spinlock lock;
4277   struct buf buf[NBUF];
4278 
4279   // Linked list of all buffers, through prev/next.
4280   // Sorted by how recently the buffer was used.
4281   // head.next is most recent, head.prev is least.
4282   struct buf head;
4283 } bcache;
4284 
4285 void
4286 binit(void)
4287 {
4288   struct buf *b;
4289 
4290   initlock(&bcache.lock, "bcache");
4291 
4292   // Create linked list of buffers
4293   bcache.head.prev = &bcache.head;
4294   bcache.head.next = &bcache.head;
4295   for(b = bcache.buf; b < bcache.buf+NBUF; b++){
4296     b->next = bcache.head.next;
4297     b->prev = &bcache.head;
4298     initsleeplock(&b->lock, "buffer");
4299     bcache.head.next->prev = b;
4300     bcache.head.next = b;
4301   }
4302 }
4303 
4304 // Look through buffer cache for block on device dev.
4305 // If not found, allocate a buffer.
4306 // In either case, return locked buffer.
4307 static struct buf*
4308 bget(uint dev, uint blockno)
4309 {
4310   struct buf *b;
4311 
4312   acquire(&bcache.lock);
4313 
4314   // Is the block already cached?
4315   for(b = bcache.head.next; b != &bcache.head; b = b->next){
4316     if(b->dev == dev && b->blockno == blockno){
4317       b->refcnt++;
4318       release(&bcache.lock);
4319       acquiresleep(&b->lock);
4320       return b;
4321     }
4322   }
4323 
4324   // Not cached.
4325   // Recycle the least recently used (LRU) unused buffer.
4326   for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
4327     if(b->refcnt == 0) {
4328       b->dev = dev;
4329       b->blockno = blockno;
4330       b->valid = 0;
4331       b->refcnt = 1;
4332       release(&bcache.lock);
4333       acquiresleep(&b->lock);
4334       return b;
4335     }
4336   }
4337   panic("bget: no buffers");
4338 }
4339 
4340 
4341 
4342 
4343 
4344 
4345 
4346 
4347 
4348 
4349 
4350 // Return a locked buf with the contents of the indicated block.
4351 struct buf*
4352 bread(uint dev, uint blockno)
4353 {
4354   struct buf *b;
4355 
4356   b = bget(dev, blockno);
4357   if(!b->valid) {
4358     virtio_disk_rw(b, 0);
4359     b->valid = 1;
4360   }
4361   return b;
4362 }
4363 
4364 // Write b's contents to disk.  Must be locked.
4365 void
4366 bwrite(struct buf *b)
4367 {
4368   if(!holdingsleep(&b->lock))
4369     panic("bwrite");
4370   virtio_disk_rw(b, 1);
4371 }
4372 
4373 // Release a locked buffer.
4374 // Move to the head of the most-recently-used list.
4375 void
4376 brelse(struct buf *b)
4377 {
4378   if(!holdingsleep(&b->lock))
4379     panic("brelse");
4380 
4381   releasesleep(&b->lock);
4382 
4383   acquire(&bcache.lock);
4384   b->refcnt--;
4385   if (b->refcnt == 0) {
4386     // no one is waiting for it.
4387     b->next->prev = b->prev;
4388     b->prev->next = b->next;
4389     b->next = bcache.head.next;
4390     b->prev = &bcache.head;
4391     bcache.head.next->prev = b;
4392     bcache.head.next = b;
4393   }
4394 
4395   release(&bcache.lock);
4396 }
4397 
4398 
4399 
4400 void
4401 bpin(struct buf *b) {
4402   acquire(&bcache.lock);
4403   b->refcnt++;
4404   release(&bcache.lock);
4405 }
4406 
4407 void
4408 bunpin(struct buf *b) {
4409   acquire(&bcache.lock);
4410   b->refcnt--;
4411   release(&bcache.lock);
4412 }
4413 
4414 
4415 
4416 
4417 
4418 
4419 
4420 
4421 
4422 
4423 
4424 
4425 
4426 
4427 
4428 
4429 
4430 
4431 
4432 
4433 
4434 
4435 
4436 
4437 
4438 
4439 
4440 
4441 
4442 
4443 
4444 
4445 
4446 
4447 
4448 
4449 

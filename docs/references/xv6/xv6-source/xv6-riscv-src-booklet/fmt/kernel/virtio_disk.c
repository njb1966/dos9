7400 //
7401 // driver for qemu's virtio disk device.
7402 // uses qemu's mmio interface to virtio.
7403 //
7404 // qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
7405 //
7406 
7407 #include "types.h"
7408 #include "riscv.h"
7409 #include "defs.h"
7410 #include "param.h"
7411 #include "memlayout.h"
7412 #include "spinlock.h"
7413 #include "sleeplock.h"
7414 #include "fs.h"
7415 #include "buf.h"
7416 #include "virtio.h"
7417 
7418 // the address of virtio mmio register r.
7419 #define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))
7420 
7421 static struct disk {
7422   // a set (not a ring) of DMA descriptors, with which the
7423   // driver tells the device where to read and write individual
7424   // disk operations. there are NUM descriptors.
7425   // most commands consist of a "chain" (a linked list) of a couple of
7426   // these descriptors.
7427   struct virtq_desc *desc;
7428 
7429   // a ring in which the driver writes descriptor numbers
7430   // that the driver would like the device to process.  it only
7431   // includes the head descriptor of each chain. the ring has
7432   // NUM elements.
7433   struct virtq_avail *avail;
7434 
7435   // a ring in which the device writes descriptor numbers that
7436   // the device has finished processing (just the head of each chain).
7437   // there are NUM used ring entries.
7438   struct virtq_used *used;
7439 
7440   // our own book-keeping.
7441   char free[NUM];  // is a descriptor free?
7442   uint16 used_idx; // we've looked this far in used[2..NUM].
7443 
7444   // track info about in-flight operations,
7445   // for use when completion interrupt arrives.
7446   // indexed by first descriptor index of chain.
7447   struct {
7448     struct buf *b;
7449     char status;
7450   } info[NUM];
7451 
7452   // disk command headers.
7453   // one-for-one with descriptors, for convenience.
7454   struct virtio_blk_req ops[NUM];
7455 
7456   struct spinlock vdisk_lock;
7457 
7458 } disk;
7459 
7460 void
7461 virtio_disk_init(void)
7462 {
7463   uint32 status = 0;
7464 
7465   initlock(&disk.vdisk_lock, "virtio_disk");
7466 
7467   if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
7468      *R(VIRTIO_MMIO_VERSION) != 2 ||
7469      *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
7470      *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
7471     panic("could not find virtio disk");
7472   }
7473 
7474   // reset device
7475   *R(VIRTIO_MMIO_STATUS) = status;
7476 
7477   // set ACKNOWLEDGE status bit
7478   status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
7479   *R(VIRTIO_MMIO_STATUS) = status;
7480 
7481   // set DRIVER status bit
7482   status |= VIRTIO_CONFIG_S_DRIVER;
7483   *R(VIRTIO_MMIO_STATUS) = status;
7484 
7485   // negotiate features
7486   uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
7487   features &= ~(1 << VIRTIO_BLK_F_RO);
7488   features &= ~(1 << VIRTIO_BLK_F_SCSI);
7489   features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
7490   features &= ~(1 << VIRTIO_BLK_F_MQ);
7491   features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
7492   features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
7493   features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
7494   *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;
7495 
7496   // tell device that feature negotiation is complete.
7497   status |= VIRTIO_CONFIG_S_FEATURES_OK;
7498   *R(VIRTIO_MMIO_STATUS) = status;
7499 
7500   // re-read status to ensure FEATURES_OK is set.
7501   status = *R(VIRTIO_MMIO_STATUS);
7502   if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
7503     panic("virtio disk FEATURES_OK unset");
7504 
7505   // initialize queue 0.
7506   *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
7507 
7508   // ensure queue 0 is not in use.
7509   if(*R(VIRTIO_MMIO_QUEUE_READY))
7510     panic("virtio disk should not be ready");
7511 
7512   // check maximum queue size.
7513   uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
7514   if(max == 0)
7515     panic("virtio disk has no queue 0");
7516   if(max < NUM)
7517     panic("virtio disk max queue too short");
7518 
7519   // allocate and zero queue memory.
7520   disk.desc = kalloc();
7521   disk.avail = kalloc();
7522   disk.used = kalloc();
7523   if(!disk.desc || !disk.avail || !disk.used)
7524     panic("virtio disk kalloc");
7525   memset(disk.desc, 0, PGSIZE);
7526   memset(disk.avail, 0, PGSIZE);
7527   memset(disk.used, 0, PGSIZE);
7528 
7529   // set queue size.
7530   *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
7531 
7532   // write physical addresses.
7533   *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)disk.desc;
7534   *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)disk.desc >> 32;
7535   *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)disk.avail;
7536   *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)disk.avail >> 32;
7537   *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)disk.used;
7538   *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)disk.used >> 32;
7539 
7540   // queue is ready.
7541   *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;
7542 
7543   // all NUM descriptors start out unused.
7544   for(int i = 0; i < NUM; i++)
7545     disk.free[i] = 1;
7546 
7547   // tell device we're completely ready.
7548   status |= VIRTIO_CONFIG_S_DRIVER_OK;
7549   *R(VIRTIO_MMIO_STATUS) = status;
7550   // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
7551 }
7552 
7553 // find a free descriptor, mark it non-free, return its index.
7554 static int
7555 alloc_desc()
7556 {
7557   for(int i = 0; i < NUM; i++){
7558     if(disk.free[i]){
7559       disk.free[i] = 0;
7560       return i;
7561     }
7562   }
7563   return -1;
7564 }
7565 
7566 // mark a descriptor as free.
7567 static void
7568 free_desc(int i)
7569 {
7570   if(i >= NUM)
7571     panic("free_desc 1");
7572   if(disk.free[i])
7573     panic("free_desc 2");
7574   disk.desc[i].addr = 0;
7575   disk.desc[i].len = 0;
7576   disk.desc[i].flags = 0;
7577   disk.desc[i].next = 0;
7578   disk.free[i] = 1;
7579   wakeup(&disk.free[0]);
7580 }
7581 
7582 // free a chain of descriptors.
7583 static void
7584 free_chain(int i)
7585 {
7586   while(1){
7587     int flag = disk.desc[i].flags;
7588     int nxt = disk.desc[i].next;
7589     free_desc(i);
7590     if(flag & VRING_DESC_F_NEXT)
7591       i = nxt;
7592     else
7593       break;
7594   }
7595 }
7596 
7597 
7598 
7599 
7600 // allocate three descriptors (they need not be contiguous).
7601 // disk transfers always use three descriptors.
7602 static int
7603 alloc3_desc(int *idx)
7604 {
7605   for(int i = 0; i < 3; i++){
7606     idx[i] = alloc_desc();
7607     if(idx[i] < 0){
7608       for(int j = 0; j < i; j++)
7609         free_desc(idx[j]);
7610       return -1;
7611     }
7612   }
7613   return 0;
7614 }
7615 
7616 void
7617 virtio_disk_rw(struct buf *b, int write)
7618 {
7619   uint64 sector = b->blockno * (BSIZE / 512);
7620 
7621   acquire(&disk.vdisk_lock);
7622 
7623   // the spec's Section 5.2 says that legacy block operations use
7624   // three descriptors: one for type/reserved/sector, one for the
7625   // data, one for a 1-byte status result.
7626 
7627   // allocate the three descriptors.
7628   int idx[3];
7629   while(1){
7630     if(alloc3_desc(idx) == 0) {
7631       break;
7632     }
7633     sleep(&disk.free[0], &disk.vdisk_lock);
7634   }
7635 
7636   // format the three descriptors.
7637   // qemu's virtio-blk.c reads them.
7638 
7639   struct virtio_blk_req *buf0 = &disk.ops[idx[0]];
7640 
7641   if(write)
7642     buf0->type = VIRTIO_BLK_T_OUT; // write the disk
7643   else
7644     buf0->type = VIRTIO_BLK_T_IN; // read the disk
7645   buf0->reserved = 0;
7646   buf0->sector = sector;
7647 
7648 
7649 
7650   disk.desc[idx[0]].addr = (uint64) buf0;
7651   disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
7652   disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
7653   disk.desc[idx[0]].next = idx[1];
7654 
7655   disk.desc[idx[1]].addr = (uint64) b->data;
7656   disk.desc[idx[1]].len = BSIZE;
7657   if(write)
7658     disk.desc[idx[1]].flags = 0; // device reads b->data
7659   else
7660     disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // device writes b->data
7661   disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
7662   disk.desc[idx[1]].next = idx[2];
7663 
7664   disk.info[idx[0]].status = 0xff; // device writes 0 on success
7665   disk.desc[idx[2]].addr = (uint64) &disk.info[idx[0]].status;
7666   disk.desc[idx[2]].len = 1;
7667   disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
7668   disk.desc[idx[2]].next = 0;
7669 
7670   // record struct buf for virtio_disk_intr().
7671   b->disk = 1;
7672   disk.info[idx[0]].b = b;
7673 
7674   // tell the device the first index in our chain of descriptors.
7675   disk.avail->ring[disk.avail->idx % NUM] = idx[0];
7676 
7677   __sync_synchronize();
7678 
7679   // tell the device another avail ring entry is available.
7680   disk.avail->idx += 1; // not % NUM ...
7681 
7682   __sync_synchronize();
7683 
7684   *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number
7685 
7686   // Wait for virtio_disk_intr() to say request has finished.
7687   while(b->disk == 1) {
7688     sleep(b, &disk.vdisk_lock);
7689   }
7690 
7691   disk.info[idx[0]].b = 0;
7692   free_chain(idx[0]);
7693 
7694   release(&disk.vdisk_lock);
7695 }
7696 
7697 
7698 
7699 
7700 void
7701 virtio_disk_intr()
7702 {
7703   acquire(&disk.vdisk_lock);
7704 
7705   // the device won't raise another interrupt until we tell it
7706   // we've seen this interrupt, which the following line does.
7707   // this may race with the device writing new entries to
7708   // the "used" ring, in which case we may process the new
7709   // completion entries in this interrupt, and have nothing to do
7710   // in the next interrupt, which is harmless.
7711   *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;
7712 
7713   __sync_synchronize();
7714 
7715   // the device increments disk.used->idx when it
7716   // adds an entry to the used ring.
7717 
7718   while(disk.used_idx != disk.used->idx){
7719     __sync_synchronize();
7720     int id = disk.used->ring[disk.used_idx % NUM].id;
7721 
7722     if(disk.info[id].status != 0)
7723       panic("virtio_disk_intr status");
7724 
7725     struct buf *b = disk.info[id].b;
7726     b->disk = 0;   // disk is done with buf
7727     wakeup(b);
7728 
7729     disk.used_idx += 1;
7730   }
7731 
7732   release(&disk.vdisk_lock);
7733 }
7734 
7735 
7736 
7737 
7738 
7739 
7740 
7741 
7742 
7743 
7744 
7745 
7746 
7747 
7748 
7749 

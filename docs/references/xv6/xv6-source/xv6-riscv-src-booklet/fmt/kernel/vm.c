1400 #include "param.h"
1401 #include "types.h"
1402 #include "memlayout.h"
1403 #include "elf.h"
1404 #include "riscv.h"
1405 #include "defs.h"
1406 #include "spinlock.h"
1407 #include "proc.h"
1408 #include "fs.h"
1409 
1410 /*
1411  * the kernel's page table.
1412  */
1413 pagetable_t kernel_pagetable;
1414 
1415 extern char etext[];  // kernel.ld sets this to end of kernel code.
1416 
1417 extern char trampoline[]; // trampoline.S
1418 
1419 // Make a direct-map page table for the kernel.
1420 pagetable_t
1421 kvmmake(void)
1422 {
1423   pagetable_t kpgtbl;
1424 
1425   kpgtbl = (pagetable_t) kalloc();
1426   memset(kpgtbl, 0, PGSIZE);
1427 
1428   // uart registers
1429   kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);
1430 
1431   // virtio mmio disk interface
1432   kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
1433 
1434   // PLIC
1435   kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);
1436 
1437   // map kernel text executable and read-only.
1438   kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
1439 
1440   // map kernel data and the physical RAM we'll make use of.
1441   kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
1442 
1443   // map the trampoline for trap entry/exit to
1444   // the highest virtual address in the kernel.
1445   kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
1446 
1447   // allocate and map a kernel stack for each process.
1448   proc_mapstacks(kpgtbl);
1449 
1450   return kpgtbl;
1451 }
1452 
1453 // add a mapping to the kernel page table.
1454 // only used when booting.
1455 // does not flush TLB or enable paging.
1456 void
1457 kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
1458 {
1459   if(mappages(kpgtbl, va, sz, pa, perm) != 0)
1460     panic("kvmmap");
1461 }
1462 
1463 // Initialize the kernel_pagetable, shared by all CPUs.
1464 void
1465 kvminit(void)
1466 {
1467   kernel_pagetable = kvmmake();
1468 }
1469 
1470 // Switch the current CPU's h/w page table register to
1471 // the kernel's page table, and enable paging.
1472 void
1473 kvminithart()
1474 {
1475   // wait for any previous writes to the page table memory to finish.
1476   sfence_vma();
1477 
1478   w_satp(MAKE_SATP(kernel_pagetable));
1479 
1480   // flush stale entries from the TLB.
1481   sfence_vma();
1482 }
1483 
1484 // Return the address of the PTE in page table pagetable
1485 // that corresponds to virtual address va.  If alloc!=0,
1486 // create any required page-table pages.
1487 //
1488 // The risc-v Sv39 scheme has three levels of page-table
1489 // pages. A page-table page contains 512 64-bit PTEs.
1490 // A 64-bit virtual address is split into five fields:
1491 //   39..63 -- must be zero.
1492 //   30..38 -- 9 bits of level-2 index.
1493 //   21..29 -- 9 bits of level-1 index.
1494 //   12..20 -- 9 bits of level-0 index.
1495 //    0..11 -- 12 bits of byte offset within the page.
1496 pte_t *
1497 walk(pagetable_t pagetable, uint64 va, int alloc)
1498 {
1499   if(va >= MAXVA)
1500     panic("walk");
1501 
1502   for(int level = 2; level > 0; level--) {
1503     pte_t *pte = &pagetable[PX(level, va)];
1504     if(*pte & PTE_V) {
1505       pagetable = (pagetable_t)PTE2PA(*pte);
1506     } else {
1507       if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
1508         return 0;
1509       memset(pagetable, 0, PGSIZE);
1510       *pte = PA2PTE(pagetable) | PTE_V;
1511     }
1512   }
1513   return &pagetable[PX(0, va)];
1514 }
1515 
1516 // Look up a virtual address, return the physical address,
1517 // or 0 if not mapped.
1518 // Can only be used to look up user pages.
1519 uint64
1520 walkaddr(pagetable_t pagetable, uint64 va)
1521 {
1522   pte_t *pte;
1523   uint64 pa;
1524 
1525   if(va >= MAXVA)
1526     return 0;
1527 
1528   pte = walk(pagetable, va, 0);
1529   if(pte == 0)
1530     return 0;
1531   if((*pte & PTE_V) == 0)
1532     return 0;
1533   if((*pte & PTE_U) == 0)
1534     return 0;
1535   pa = PTE2PA(*pte);
1536   return pa;
1537 }
1538 
1539 
1540 
1541 
1542 
1543 
1544 
1545 
1546 
1547 
1548 
1549 
1550 // Create PTEs for virtual addresses starting at va that refer to
1551 // physical addresses starting at pa.
1552 // va and size MUST be page-aligned.
1553 // Returns 0 on success, -1 if walk() couldn't
1554 // allocate a needed page-table page.
1555 int
1556 mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
1557 {
1558   uint64 a, last;
1559   pte_t *pte;
1560 
1561   if((va % PGSIZE) != 0)
1562     panic("mappages: va not aligned");
1563 
1564   if((size % PGSIZE) != 0)
1565     panic("mappages: size not aligned");
1566 
1567   if(size == 0)
1568     panic("mappages: size");
1569 
1570   a = va;
1571   last = va + size - PGSIZE;
1572   for(;;){
1573     if((pte = walk(pagetable, a, 1)) == 0)
1574       return -1;
1575     if(*pte & PTE_V)
1576       panic("mappages: remap");
1577     *pte = PA2PTE(pa) | perm | PTE_V;
1578     if(a == last)
1579       break;
1580     a += PGSIZE;
1581     pa += PGSIZE;
1582   }
1583   return 0;
1584 }
1585 
1586 // create an empty user page table.
1587 // returns 0 if out of memory.
1588 pagetable_t
1589 uvmcreate()
1590 {
1591   pagetable_t pagetable;
1592   pagetable = (pagetable_t) kalloc();
1593   if(pagetable == 0)
1594     return 0;
1595   memset(pagetable, 0, PGSIZE);
1596   return pagetable;
1597 }
1598 
1599 
1600 // Remove npages of mappings starting from va. va must be
1601 // page-aligned. It's OK if the mappings don't exist.
1602 // Optionally free the physical memory.
1603 void
1604 uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
1605 {
1606   uint64 a;
1607   pte_t *pte;
1608 
1609   if((va % PGSIZE) != 0)
1610     panic("uvmunmap: not aligned");
1611 
1612   for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
1613     if((pte = walk(pagetable, a, 0)) == 0) // leaf page table entry allocated?
1614       continue;
1615     if((*pte & PTE_V) == 0)  // has physical page been allocated?
1616       continue;
1617     if(do_free){
1618       uint64 pa = PTE2PA(*pte);
1619       kfree((void*)pa);
1620     }
1621     *pte = 0;
1622   }
1623 }
1624 
1625 // Allocate PTEs and physical memory to grow a process from oldsz to
1626 // newsz, which need not be page aligned.  Returns new size or 0 on error.
1627 uint64
1628 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
1629 {
1630   char *mem;
1631   uint64 a;
1632 
1633   if(newsz < oldsz)
1634     return oldsz;
1635 
1636   oldsz = PGROUNDUP(oldsz);
1637   for(a = oldsz; a < newsz; a += PGSIZE){
1638     mem = kalloc();
1639     if(mem == 0){
1640       uvmdealloc(pagetable, a, oldsz);
1641       return 0;
1642     }
1643     memset(mem, 0, PGSIZE);
1644     if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
1645       kfree(mem);
1646       uvmdealloc(pagetable, a, oldsz);
1647       return 0;
1648     }
1649   }
1650   return newsz;
1651 }
1652 
1653 // Deallocate user pages to bring the process size from oldsz to
1654 // newsz.  oldsz and newsz need not be page-aligned, nor does newsz
1655 // need to be less than oldsz.  oldsz can be larger than the actual
1656 // process size.  Returns the new process size.
1657 uint64
1658 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
1659 {
1660   if(newsz >= oldsz)
1661     return oldsz;
1662 
1663   if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
1664     int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
1665     uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
1666   }
1667 
1668   return newsz;
1669 }
1670 
1671 // Recursively free page-table pages.
1672 // All leaf mappings must already have been removed.
1673 void
1674 freewalk(pagetable_t pagetable)
1675 {
1676   // there are 2^9 = 512 PTEs in a page table.
1677   for(int i = 0; i < 512; i++){
1678     pte_t pte = pagetable[i];
1679     if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
1680       // this PTE points to a lower-level page table.
1681       uint64 child = PTE2PA(pte);
1682       freewalk((pagetable_t)child);
1683       pagetable[i] = 0;
1684     } else if(pte & PTE_V){
1685       panic("freewalk: leaf");
1686     }
1687   }
1688   kfree((void*)pagetable);
1689 }
1690 
1691 // Free user memory pages,
1692 // then free page-table pages.
1693 void
1694 uvmfree(pagetable_t pagetable, uint64 sz)
1695 {
1696   if(sz > 0)
1697     uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
1698   freewalk(pagetable);
1699 }
1700 // Given a parent process's page table, copy
1701 // its memory into a child's page table.
1702 // Copies both the page table and the
1703 // physical memory.
1704 // returns 0 on success, -1 on failure.
1705 // frees any allocated pages on failure.
1706 int
1707 uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
1708 {
1709   pte_t *pte;
1710   uint64 pa, i;
1711   uint flags;
1712   char *mem;
1713 
1714   for(i = 0; i < sz; i += PGSIZE){
1715     if((pte = walk(old, i, 0)) == 0)
1716       continue;   // page table entry hasn't been allocated
1717     if((*pte & PTE_V) == 0)
1718       continue;   // physical page hasn't been allocated
1719     pa = PTE2PA(*pte);
1720     flags = PTE_FLAGS(*pte);
1721     if((mem = kalloc()) == 0)
1722       goto err;
1723     memmove(mem, (char*)pa, PGSIZE);
1724     if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
1725       kfree(mem);
1726       goto err;
1727     }
1728   }
1729   return 0;
1730 
1731  err:
1732   uvmunmap(new, 0, i / PGSIZE, 1);
1733   return -1;
1734 }
1735 
1736 // mark a PTE invalid for user access.
1737 // used by exec for the user stack guard page.
1738 void
1739 uvmclear(pagetable_t pagetable, uint64 va)
1740 {
1741   pte_t *pte;
1742 
1743   pte = walk(pagetable, va, 0);
1744   if(pte == 0)
1745     panic("uvmclear");
1746   *pte &= ~PTE_U;
1747 }
1748 
1749 
1750 // Copy from kernel to user.
1751 // Copy len bytes from src to virtual address dstva in a given page table.
1752 // Return 0 on success, -1 on error.
1753 int
1754 copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
1755 {
1756   uint64 n, va0, pa0;
1757   pte_t *pte;
1758 
1759   while(len > 0){
1760     va0 = PGROUNDDOWN(dstva);
1761     if(va0 >= MAXVA)
1762       return -1;
1763 
1764     pa0 = walkaddr(pagetable, va0);
1765     if(pa0 == 0) {
1766       if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
1767         return -1;
1768       }
1769     }
1770 
1771     pte = walk(pagetable, va0, 0);
1772     // forbid copyout over read-only user text pages.
1773     if((*pte & PTE_W) == 0)
1774       return -1;
1775 
1776     n = PGSIZE - (dstva - va0);
1777     if(n > len)
1778       n = len;
1779     memmove((void *)(pa0 + (dstva - va0)), src, n);
1780 
1781     len -= n;
1782     src += n;
1783     dstva = va0 + PGSIZE;
1784   }
1785   return 0;
1786 }
1787 
1788 
1789 
1790 
1791 
1792 
1793 
1794 
1795 
1796 
1797 
1798 
1799 
1800 // Copy from user to kernel.
1801 // Copy len bytes to dst from virtual address srcva in a given page table.
1802 // Return 0 on success, -1 on error.
1803 int
1804 copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
1805 {
1806   uint64 n, va0, pa0;
1807 
1808   while(len > 0){
1809     va0 = PGROUNDDOWN(srcva);
1810     pa0 = walkaddr(pagetable, va0);
1811     if(pa0 == 0) {
1812       if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
1813         return -1;
1814       }
1815     }
1816     n = PGSIZE - (srcva - va0);
1817     if(n > len)
1818       n = len;
1819     memmove(dst, (void *)(pa0 + (srcva - va0)), n);
1820 
1821     len -= n;
1822     dst += n;
1823     srcva = va0 + PGSIZE;
1824   }
1825   return 0;
1826 }
1827 
1828 // Copy a null-terminated string from user to kernel.
1829 // Copy bytes to dst from virtual address srcva in a given page table,
1830 // until a '\0', or max.
1831 // Return 0 on success, -1 on error.
1832 int
1833 copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
1834 {
1835   uint64 n, va0, pa0;
1836   int got_null = 0;
1837 
1838   while(got_null == 0 && max > 0){
1839     va0 = PGROUNDDOWN(srcva);
1840     pa0 = walkaddr(pagetable, va0);
1841     if(pa0 == 0)
1842       return -1;
1843     n = PGSIZE - (srcva - va0);
1844     if(n > max)
1845       n = max;
1846 
1847 
1848 
1849 
1850     char *p = (char *) (pa0 + (srcva - va0));
1851     while(n > 0){
1852       if(*p == '\0'){
1853         *dst = '\0';
1854         got_null = 1;
1855         break;
1856       } else {
1857         *dst = *p;
1858       }
1859       --n;
1860       --max;
1861       p++;
1862       dst++;
1863     }
1864 
1865     srcva = va0 + PGSIZE;
1866   }
1867   if(got_null){
1868     return 0;
1869   } else {
1870     return -1;
1871   }
1872 }
1873 
1874 // allocate and map user memory if process is referencing a page
1875 // that was lazily allocated in sys_sbrk().
1876 // returns 0 if va is invalid or already mapped, or if
1877 // out of physical memory, and physical address if successful.
1878 uint64
1879 vmfault(pagetable_t pagetable, uint64 va, int read)
1880 {
1881   uint64 mem;
1882   struct proc *p = myproc();
1883 
1884   if (va >= p->sz)
1885     return 0;
1886   va = PGROUNDDOWN(va);
1887   if(ismapped(pagetable, va)) {
1888     return 0;
1889   }
1890   mem = (uint64) kalloc();
1891   if(mem == 0)
1892     return 0;
1893   memset((void *) mem, 0, PGSIZE);
1894   if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W|PTE_U|PTE_R) != 0) {
1895     kfree((void *)mem);
1896     return 0;
1897   }
1898   return mem;
1899 }
1900 int
1901 ismapped(pagetable_t pagetable, uint64 va)
1902 {
1903   pte_t *pte = walk(pagetable, va, 0);
1904   if (pte == 0) {
1905     return 0;
1906   }
1907   if (*pte & PTE_V){
1908     return 1;
1909   }
1910   return 0;
1911 }
1912 
1913 
1914 
1915 
1916 
1917 
1918 
1919 
1920 
1921 
1922 
1923 
1924 
1925 
1926 
1927 
1928 
1929 
1930 
1931 
1932 
1933 
1934 
1935 
1936 
1937 
1938 
1939 
1940 
1941 
1942 
1943 
1944 
1945 
1946 
1947 
1948 
1949 

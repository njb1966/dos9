2100 #include "types.h"
2101 #include "param.h"
2102 #include "memlayout.h"
2103 #include "riscv.h"
2104 #include "spinlock.h"
2105 #include "proc.h"
2106 #include "defs.h"
2107 
2108 struct cpu cpus[NCPU];
2109 
2110 struct proc proc[NPROC];
2111 
2112 struct proc *initproc;
2113 
2114 int nextpid = 1;
2115 struct spinlock pid_lock;
2116 
2117 extern void forkret(void);
2118 static void freeproc(struct proc *p);
2119 
2120 extern char trampoline[]; // trampoline.S
2121 
2122 // helps ensure that wakeups of wait()ing
2123 // parents are not lost. helps obey the
2124 // memory model when using p->parent.
2125 // must be acquired before any p->lock.
2126 struct spinlock wait_lock;
2127 
2128 // Allocate a page for each process's kernel stack.
2129 // Map it high in memory, followed by an invalid
2130 // guard page.
2131 void
2132 proc_mapstacks(pagetable_t kpgtbl)
2133 {
2134   struct proc *p;
2135 
2136   for(p = proc; p < &proc[NPROC]; p++) {
2137     char *pa = kalloc();
2138     if(pa == 0)
2139       panic("kalloc");
2140     uint64 va = KSTACK((int) (p - proc));
2141     kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
2142   }
2143 }
2144 
2145 
2146 
2147 
2148 
2149 
2150 // initialize the proc table.
2151 void
2152 procinit(void)
2153 {
2154   struct proc *p;
2155 
2156   initlock(&pid_lock, "nextpid");
2157   initlock(&wait_lock, "wait_lock");
2158   for(p = proc; p < &proc[NPROC]; p++) {
2159       initlock(&p->lock, "proc");
2160       p->state = UNUSED;
2161       p->kstack = KSTACK((int) (p - proc));
2162   }
2163 }
2164 
2165 // Must be called with interrupts disabled,
2166 // to prevent race with process being moved
2167 // to a different CPU.
2168 int
2169 cpuid()
2170 {
2171   int id = r_tp();
2172   return id;
2173 }
2174 
2175 // Return this CPU's cpu struct.
2176 // Interrupts must be disabled.
2177 struct cpu*
2178 mycpu(void)
2179 {
2180   int id = cpuid();
2181   struct cpu *c = &cpus[id];
2182   return c;
2183 }
2184 
2185 // Return the current struct proc *, or zero if none.
2186 struct proc*
2187 myproc(void)
2188 {
2189   push_off();
2190   struct cpu *c = mycpu();
2191   struct proc *p = c->proc;
2192   pop_off();
2193   return p;
2194 }
2195 
2196 
2197 
2198 
2199 
2200 int
2201 allocpid()
2202 {
2203   int pid;
2204 
2205   acquire(&pid_lock);
2206   pid = nextpid;
2207   nextpid = nextpid + 1;
2208   release(&pid_lock);
2209 
2210   return pid;
2211 }
2212 
2213 // Look in the process table for an UNUSED proc.
2214 // If found, initialize state required to run in the kernel,
2215 // and return with p->lock held.
2216 // If there are no free procs, or a memory allocation fails, return 0.
2217 static struct proc*
2218 allocproc(void)
2219 {
2220   struct proc *p;
2221 
2222   for(p = proc; p < &proc[NPROC]; p++) {
2223     acquire(&p->lock);
2224     if(p->state == UNUSED) {
2225       goto found;
2226     } else {
2227       release(&p->lock);
2228     }
2229   }
2230   return 0;
2231 
2232 found:
2233   p->pid = allocpid();
2234   p->state = USED;
2235 
2236   // Allocate a trapframe page.
2237   if((p->trapframe = (struct trapframe *)kalloc()) == 0){
2238     freeproc(p);
2239     release(&p->lock);
2240     return 0;
2241   }
2242 
2243   // An empty user page table.
2244   p->pagetable = proc_pagetable(p);
2245   if(p->pagetable == 0){
2246     freeproc(p);
2247     release(&p->lock);
2248     return 0;
2249   }
2250   // Set up new context to start executing at forkret,
2251   // which returns to user space.
2252   memset(&p->context, 0, sizeof(p->context));
2253   p->context.ra = (uint64)forkret;
2254   p->context.sp = p->kstack + PGSIZE;
2255 
2256   return p;
2257 }
2258 
2259 // free a proc structure and the data hanging from it,
2260 // including user pages.
2261 // p->lock must be held.
2262 static void
2263 freeproc(struct proc *p)
2264 {
2265   if(p->trapframe)
2266     kfree((void*)p->trapframe);
2267   p->trapframe = 0;
2268   if(p->pagetable)
2269     proc_freepagetable(p->pagetable, p->sz);
2270   p->pagetable = 0;
2271   p->sz = 0;
2272   p->pid = 0;
2273   p->parent = 0;
2274   p->name[0] = 0;
2275   p->chan = 0;
2276   p->killed = 0;
2277   p->xstate = 0;
2278   p->state = UNUSED;
2279 }
2280 
2281 // Create a user page table for a given process, with no user memory,
2282 // but with trampoline and trapframe pages.
2283 pagetable_t
2284 proc_pagetable(struct proc *p)
2285 {
2286   pagetable_t pagetable;
2287 
2288   // An empty page table.
2289   pagetable = uvmcreate();
2290   if(pagetable == 0)
2291     return 0;
2292 
2293   // map the trampoline code (for system call return)
2294   // at the highest user virtual address.
2295   // only the supervisor uses it, on the way
2296   // to/from user space, so not PTE_U.
2297   if(mappages(pagetable, TRAMPOLINE, PGSIZE,
2298               (uint64)trampoline, PTE_R | PTE_X) < 0){
2299     uvmfree(pagetable, 0);
2300     return 0;
2301   }
2302 
2303   // map the trapframe page just below the trampoline page, for
2304   // trampoline.S.
2305   if(mappages(pagetable, TRAPFRAME, PGSIZE,
2306               (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
2307     uvmunmap(pagetable, TRAMPOLINE, 1, 0);
2308     uvmfree(pagetable, 0);
2309     return 0;
2310   }
2311 
2312   return pagetable;
2313 }
2314 
2315 // Free a process's page table, and free the
2316 // physical memory it refers to.
2317 void
2318 proc_freepagetable(pagetable_t pagetable, uint64 sz)
2319 {
2320   uvmunmap(pagetable, TRAMPOLINE, 1, 0);
2321   uvmunmap(pagetable, TRAPFRAME, 1, 0);
2322   uvmfree(pagetable, sz);
2323 }
2324 
2325 // Set up first user process.
2326 void
2327 userinit(void)
2328 {
2329   struct proc *p;
2330 
2331   p = allocproc();
2332   initproc = p;
2333 
2334   p->cwd = namei("/");
2335 
2336   p->state = RUNNABLE;
2337 
2338   release(&p->lock);
2339 }
2340 
2341 
2342 
2343 
2344 
2345 
2346 
2347 
2348 
2349 
2350 // Grow or shrink user memory by n bytes.
2351 // Return 0 on success, -1 on failure.
2352 int
2353 growproc(int n)
2354 {
2355   uint64 sz;
2356   struct proc *p = myproc();
2357 
2358   sz = p->sz;
2359   if(n > 0){
2360     if(sz + n > TRAPFRAME) {
2361       return -1;
2362     }
2363     if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
2364       return -1;
2365     }
2366   } else if(n < 0){
2367     sz = uvmdealloc(p->pagetable, sz, sz + n);
2368   }
2369   p->sz = sz;
2370   return 0;
2371 }
2372 
2373 // Create a new process, copying the parent.
2374 // Sets up child kernel stack to return as if from fork() system call.
2375 int
2376 kfork(void)
2377 {
2378   int i, pid;
2379   struct proc *np;
2380   struct proc *p = myproc();
2381 
2382   // Allocate process.
2383   if((np = allocproc()) == 0){
2384     return -1;
2385   }
2386 
2387   // Copy user memory from parent to child.
2388   if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
2389     freeproc(np);
2390     release(&np->lock);
2391     return -1;
2392   }
2393   np->sz = p->sz;
2394 
2395   // copy saved user registers.
2396   *(np->trapframe) = *(p->trapframe);
2397 
2398   // Cause fork to return 0 in the child.
2399   np->trapframe->a0 = 0;
2400   // increment reference counts on open file descriptors.
2401   for(i = 0; i < NOFILE; i++)
2402     if(p->ofile[i])
2403       np->ofile[i] = filedup(p->ofile[i]);
2404   np->cwd = idup(p->cwd);
2405 
2406   safestrcpy(np->name, p->name, sizeof(p->name));
2407 
2408   pid = np->pid;
2409 
2410   release(&np->lock);
2411 
2412   acquire(&wait_lock);
2413   np->parent = p;
2414   release(&wait_lock);
2415 
2416   acquire(&np->lock);
2417   np->state = RUNNABLE;
2418   release(&np->lock);
2419 
2420   return pid;
2421 }
2422 
2423 // Pass p's abandoned children to init.
2424 // Caller must hold wait_lock.
2425 void
2426 reparent(struct proc *p)
2427 {
2428   struct proc *pp;
2429 
2430   for(pp = proc; pp < &proc[NPROC]; pp++){
2431     if(pp->parent == p){
2432       pp->parent = initproc;
2433       wakeup(initproc);
2434     }
2435   }
2436 }
2437 
2438 
2439 
2440 
2441 
2442 
2443 
2444 
2445 
2446 
2447 
2448 
2449 
2450 // Exit the current process.  Does not return.
2451 // An exited process remains in the zombie state
2452 // until its parent calls wait().
2453 void
2454 kexit(int status)
2455 {
2456   struct proc *p = myproc();
2457 
2458   if(p == initproc)
2459     panic("init exiting");
2460 
2461   // Close all open files.
2462   for(int fd = 0; fd < NOFILE; fd++){
2463     if(p->ofile[fd]){
2464       struct file *f = p->ofile[fd];
2465       fileclose(f);
2466       p->ofile[fd] = 0;
2467     }
2468   }
2469 
2470   begin_op();
2471   iput(p->cwd);
2472   end_op();
2473   p->cwd = 0;
2474 
2475   acquire(&wait_lock);
2476 
2477   // Give any children to init.
2478   reparent(p);
2479 
2480   // Parent might be sleeping in wait().
2481   wakeup(p->parent);
2482 
2483   acquire(&p->lock);
2484 
2485   p->xstate = status;
2486   p->state = ZOMBIE;
2487 
2488   release(&wait_lock);
2489 
2490   // Jump into the scheduler, never to return.
2491   sched();
2492   panic("zombie exit");
2493 }
2494 
2495 
2496 
2497 
2498 
2499 
2500 // Wait for a child process to exit and return its pid.
2501 // Return -1 if this process has no children.
2502 int
2503 kwait(uint64 addr)
2504 {
2505   struct proc *pp;
2506   int havekids, pid;
2507   struct proc *p = myproc();
2508 
2509   acquire(&wait_lock);
2510 
2511   for(;;){
2512     // Scan through table looking for exited children.
2513     havekids = 0;
2514     for(pp = proc; pp < &proc[NPROC]; pp++){
2515       if(pp->parent == p){
2516         // make sure the child isn't still in exit() or swtch().
2517         acquire(&pp->lock);
2518 
2519         havekids = 1;
2520         if(pp->state == ZOMBIE){
2521           // Found one.
2522           pid = pp->pid;
2523           if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
2524                                   sizeof(pp->xstate)) < 0) {
2525             release(&pp->lock);
2526             release(&wait_lock);
2527             return -1;
2528           }
2529           freeproc(pp);
2530           release(&pp->lock);
2531           release(&wait_lock);
2532           return pid;
2533         }
2534         release(&pp->lock);
2535       }
2536     }
2537 
2538     // No point waiting if we don't have any children.
2539     if(!havekids || killed(p)){
2540       release(&wait_lock);
2541       return -1;
2542     }
2543 
2544     // Wait for a child to exit.
2545     sleep(p, &wait_lock);  //DOC: wait-sleep
2546   }
2547 }
2548 
2549 
2550 // Per-CPU process scheduler.
2551 // Each CPU calls scheduler() after setting itself up.
2552 // Scheduler never returns.  It loops, doing:
2553 //  - choose a process to run.
2554 //  - swtch to start running that process.
2555 //  - eventually that process transfers control
2556 //    via swtch back to the scheduler.
2557 void
2558 scheduler(void)
2559 {
2560   struct proc *p;
2561   struct cpu *c = mycpu();
2562 
2563   c->proc = 0;
2564   for(;;){
2565     // The most recent process to run may have had interrupts
2566     // turned off; enable them to avoid a deadlock if all
2567     // processes are waiting. Then turn them back off
2568     // to avoid a possible race between an interrupt
2569     // and wfi.
2570     intr_on();
2571     intr_off();
2572 
2573     int found = 0;
2574     for(p = proc; p < &proc[NPROC]; p++) {
2575       acquire(&p->lock);
2576       if(p->state == RUNNABLE) {
2577         // Switch to chosen process.  It is the process's job
2578         // to release its lock and then reacquire it
2579         // before jumping back to us.
2580         p->state = RUNNING;
2581         c->proc = p;
2582         swtch(&c->context, &p->context);
2583 
2584         // Process is done running for now.
2585         // It should have changed its p->state before coming back.
2586         c->proc = 0;
2587         found = 1;
2588       }
2589       release(&p->lock);
2590     }
2591     if(found == 0) {
2592       // nothing to run; stop running on this core until an interrupt.
2593       asm volatile("wfi");
2594     }
2595   }
2596 }
2597 
2598 
2599 
2600 // Switch to scheduler.  Must hold only p->lock
2601 // and have changed proc->state. Saves and restores
2602 // intena because intena is a property of this
2603 // kernel thread, not this CPU. It should
2604 // be proc->intena and proc->noff, but that would
2605 // break in the few places where a lock is held but
2606 // there's no process.
2607 void
2608 sched(void)
2609 {
2610   int intena;
2611   struct proc *p = myproc();
2612 
2613   if(!holding(&p->lock))
2614     panic("sched p->lock");
2615   if(mycpu()->noff != 1)
2616     panic("sched locks");
2617   if(p->state == RUNNING)
2618     panic("sched RUNNING");
2619   if(intr_get())
2620     panic("sched interruptible");
2621 
2622   intena = mycpu()->intena;
2623   swtch(&p->context, &mycpu()->context);
2624   mycpu()->intena = intena;
2625 }
2626 
2627 // Give up the CPU for one scheduling round.
2628 void
2629 yield(void)
2630 {
2631   struct proc *p = myproc();
2632   acquire(&p->lock);
2633   p->state = RUNNABLE;
2634   sched();
2635   release(&p->lock);
2636 }
2637 
2638 
2639 
2640 
2641 
2642 
2643 
2644 
2645 
2646 
2647 
2648 
2649 
2650 // A fork child's very first scheduling by scheduler()
2651 // will swtch to forkret.
2652 void
2653 forkret(void)
2654 {
2655   extern char userret[];
2656   static int first = 1;
2657   struct proc *p = myproc();
2658 
2659   // Still holding p->lock from scheduler.
2660   release(&p->lock);
2661 
2662   if (first) {
2663     // File system initialization must be run in the context of a
2664     // regular process (e.g., because it calls sleep), and thus cannot
2665     // be run from main().
2666     fsinit(ROOTDEV);
2667 
2668     first = 0;
2669     // ensure other cores see first=0.
2670     __sync_synchronize();
2671 
2672     // We can invoke kexec() now that file system is initialized.
2673     // Put the return value (argc) of kexec into a0.
2674     p->trapframe->a0 = kexec("/init", (char *[]){ "/init", 0 });
2675     if (p->trapframe->a0 == -1) {
2676       panic("exec");
2677     }
2678   }
2679 
2680   // return to user space, mimicing usertrap()'s return.
2681   prepare_return();
2682   uint64 satp = MAKE_SATP(p->pagetable);
2683   uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
2684   ((void (*)(uint64))trampoline_userret)(satp);
2685 }
2686 
2687 
2688 
2689 
2690 
2691 
2692 
2693 
2694 
2695 
2696 
2697 
2698 
2699 
2700 // Sleep on channel chan, releasing condition lock lk.
2701 // Re-acquires lk when awakened.
2702 void
2703 sleep(void *chan, struct spinlock *lk)
2704 {
2705   struct proc *p = myproc();
2706 
2707   // Must acquire p->lock in order to
2708   // change p->state and then call sched.
2709   // Once we hold p->lock, we can be
2710   // guaranteed that we won't miss any wakeup
2711   // (wakeup locks p->lock),
2712   // so it's okay to release lk.
2713 
2714   acquire(&p->lock);  //DOC: sleeplock1
2715   release(lk);
2716 
2717   // Go to sleep.
2718   p->chan = chan;
2719   p->state = SLEEPING;
2720 
2721   sched();
2722 
2723   // Tidy up.
2724   p->chan = 0;
2725 
2726   // Reacquire original lock.
2727   release(&p->lock);
2728   acquire(lk);
2729 }
2730 
2731 // Wake up all processes sleeping on channel chan.
2732 // Caller should hold the condition lock.
2733 void
2734 wakeup(void *chan)
2735 {
2736   struct proc *p;
2737 
2738   for(p = proc; p < &proc[NPROC]; p++) {
2739     if(p != myproc()){
2740       acquire(&p->lock);
2741       if(p->state == SLEEPING && p->chan == chan) {
2742         p->state = RUNNABLE;
2743       }
2744       release(&p->lock);
2745     }
2746   }
2747 }
2748 
2749 
2750 // Kill the process with the given pid.
2751 // The victim won't exit until it tries to return
2752 // to user space (see usertrap() in trap.c).
2753 int
2754 kkill(int pid)
2755 {
2756   struct proc *p;
2757 
2758   for(p = proc; p < &proc[NPROC]; p++){
2759     acquire(&p->lock);
2760     if(p->pid == pid){
2761       p->killed = 1;
2762       if(p->state == SLEEPING){
2763         // Wake process from sleep().
2764         p->state = RUNNABLE;
2765       }
2766       release(&p->lock);
2767       return 0;
2768     }
2769     release(&p->lock);
2770   }
2771   return -1;
2772 }
2773 
2774 void
2775 setkilled(struct proc *p)
2776 {
2777   acquire(&p->lock);
2778   p->killed = 1;
2779   release(&p->lock);
2780 }
2781 
2782 int
2783 killed(struct proc *p)
2784 {
2785   int k;
2786 
2787   acquire(&p->lock);
2788   k = p->killed;
2789   release(&p->lock);
2790   return k;
2791 }
2792 
2793 
2794 
2795 
2796 
2797 
2798 
2799 
2800 // Copy to either a user address, or kernel address,
2801 // depending on usr_dst.
2802 // Returns 0 on success, -1 on error.
2803 int
2804 either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
2805 {
2806   struct proc *p = myproc();
2807   if(user_dst){
2808     return copyout(p->pagetable, dst, src, len);
2809   } else {
2810     memmove((char *)dst, src, len);
2811     return 0;
2812   }
2813 }
2814 
2815 // Copy from either a user address, or kernel address,
2816 // depending on usr_src.
2817 // Returns 0 on success, -1 on error.
2818 int
2819 either_copyin(void *dst, int user_src, uint64 src, uint64 len)
2820 {
2821   struct proc *p = myproc();
2822   if(user_src){
2823     return copyin(p->pagetable, dst, src, len);
2824   } else {
2825     memmove(dst, (char*)src, len);
2826     return 0;
2827   }
2828 }
2829 
2830 // Print a process listing to console.  For debugging.
2831 // Runs when user types ^P on console.
2832 // No lock to avoid wedging a stuck machine further.
2833 void
2834 procdump(void)
2835 {
2836   static char *states[] = {
2837   [UNUSED]    "unused",
2838   [USED]      "used",
2839   [SLEEPING]  "sleep ",
2840   [RUNNABLE]  "runble",
2841   [RUNNING]   "run   ",
2842   [ZOMBIE]    "zombie"
2843   };
2844   struct proc *p;
2845   char *state;
2846 
2847   printf("\n");
2848   for(p = proc; p < &proc[NPROC]; p++){
2849     if(p->state == UNUSED)
2850       continue;
2851     if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
2852       state = states[p->state];
2853     else
2854       state = "???";
2855     printf("%d %s %s", p->pid, state, p->name);
2856     printf("\n");
2857   }
2858 }
2859 
2860 
2861 
2862 
2863 
2864 
2865 
2866 
2867 
2868 
2869 
2870 
2871 
2872 
2873 
2874 
2875 
2876 
2877 
2878 
2879 
2880 
2881 
2882 
2883 
2884 
2885 
2886 
2887 
2888 
2889 
2890 
2891 
2892 
2893 
2894 
2895 
2896 
2897 
2898 
2899 

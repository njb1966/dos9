2950 // Physical memory allocator, for user processes,
2951 // kernel stacks, page-table pages,
2952 // and pipe buffers. Allocates whole 4096-byte pages.
2953 
2954 #include "types.h"
2955 #include "param.h"
2956 #include "memlayout.h"
2957 #include "spinlock.h"
2958 #include "riscv.h"
2959 #include "defs.h"
2960 
2961 void freerange(void *pa_start, void *pa_end);
2962 
2963 extern char end[]; // first address after kernel.
2964                    // defined by kernel.ld.
2965 
2966 struct run {
2967   struct run *next;
2968 };
2969 
2970 struct {
2971   struct spinlock lock;
2972   struct run *freelist;
2973 } kmem;
2974 
2975 void
2976 kinit()
2977 {
2978   initlock(&kmem.lock, "kmem");
2979   freerange(end, (void*)PHYSTOP);
2980 }
2981 
2982 void
2983 freerange(void *pa_start, void *pa_end)
2984 {
2985   char *p;
2986   p = (char*)PGROUNDUP((uint64)pa_start);
2987   for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
2988     kfree(p);
2989 }
2990 
2991 
2992 
2993 
2994 
2995 
2996 
2997 
2998 
2999 
3000 // Free the page of physical memory pointed at by pa,
3001 // which normally should have been returned by a
3002 // call to kalloc().  (The exception is when
3003 // initializing the allocator; see kinit above.)
3004 void
3005 kfree(void *pa)
3006 {
3007   struct run *r;
3008 
3009   if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
3010     panic("kfree");
3011 
3012   // Fill with junk to catch dangling refs.
3013   memset(pa, 1, PGSIZE);
3014 
3015   r = (struct run*)pa;
3016 
3017   acquire(&kmem.lock);
3018   r->next = kmem.freelist;
3019   kmem.freelist = r;
3020   release(&kmem.lock);
3021 }
3022 
3023 // Allocate one 4096-byte page of physical memory.
3024 // Returns a pointer that the kernel can use.
3025 // Returns 0 if the memory cannot be allocated.
3026 void *
3027 kalloc(void)
3028 {
3029   struct run *r;
3030 
3031   acquire(&kmem.lock);
3032   r = kmem.freelist;
3033   if(r)
3034     kmem.freelist = r->next;
3035   release(&kmem.lock);
3036 
3037   if(r)
3038     memset((char*)r, 5, PGSIZE); // fill with junk
3039   return (void*)r;
3040 }
3041 
3042 
3043 
3044 
3045 
3046 
3047 
3048 
3049 

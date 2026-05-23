0200 // Physical memory layout
0201 
0202 // qemu -machine virt is set up like this,
0203 // based on qemu's hw/riscv/virt.c:
0204 //
0205 // 00001000 -- boot ROM, provided by qemu
0206 // 02000000 -- CLINT
0207 // 0C000000 -- PLIC
0208 // 10000000 -- uart0
0209 // 10001000 -- virtio disk
0210 // 80000000 -- qemu's boot ROM loads the kernel here,
0211 //             then jumps here.
0212 // unused RAM after 80000000.
0213 
0214 // the kernel uses physical memory thus:
0215 // 80000000 -- entry.S, then kernel text and data
0216 // end -- start of kernel page allocation area
0217 // PHYSTOP -- end RAM used by the kernel
0218 
0219 // qemu puts UART registers here in physical memory.
0220 #define UART0 0x10000000L
0221 #define UART0_IRQ 10
0222 
0223 // virtio mmio interface
0224 #define VIRTIO0 0x10001000
0225 #define VIRTIO0_IRQ 1
0226 
0227 // qemu puts platform-level interrupt controller (PLIC) here.
0228 #define PLIC 0x0c000000L
0229 #define PLIC_PRIORITY (PLIC + 0x0)
0230 #define PLIC_PENDING (PLIC + 0x1000)
0231 #define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
0232 #define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
0233 #define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)
0234 
0235 // the kernel expects there to be RAM
0236 // for use by the kernel and user pages
0237 // from physical address 0x80000000 to PHYSTOP.
0238 #define KERNBASE 0x80000000L
0239 #define PHYSTOP (KERNBASE + 128*1024*1024)
0240 
0241 // map the trampoline page to the highest address,
0242 // in both user and kernel space.
0243 #define TRAMPOLINE (MAXVA - PGSIZE)
0244 
0245 // map kernel stacks beneath the trampoline,
0246 // each surrounded by invalid guard pages.
0247 #define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)
0248 
0249 
0250 // User memory layout.
0251 // Address zero first:
0252 //   text
0253 //   original data and bss
0254 //   fixed-size stack
0255 //   expandable heap
0256 //   ...
0257 //   TRAPFRAME (p->trapframe, used by the trampoline)
0258 //   TRAMPOLINE (the same page as in the kernel)
0259 #define TRAPFRAME (TRAMPOLINE - PGSIZE)
0260 
0261 
0262 
0263 
0264 
0265 
0266 
0267 
0268 
0269 
0270 
0271 
0272 
0273 
0274 
0275 
0276 
0277 
0278 
0279 
0280 
0281 
0282 
0283 
0284 
0285 
0286 
0287 
0288 
0289 
0290 
0291 
0292 
0293 
0294 
0295 
0296 
0297 
0298 
0299 

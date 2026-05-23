3050         #
3051         # low-level code to handle traps from user space into
3052         # the kernel, and returns from kernel to user.
3053         #
3054         # the kernel maps the page holding this code
3055         # at the same virtual address (TRAMPOLINE)
3056         # in user and kernel space so that it continues
3057         # to work when it switches page tables.
3058         # kernel.ld causes this code to start at
3059         # a page boundary.
3060         #
3061 
3062 #include "riscv.h"
3063 #include "memlayout.h"
3064 
3065 .section trampsec
3066 .globl trampoline
3067 .globl usertrap
3068 trampoline:
3069 .align 4
3070 .globl uservec
3071 uservec:
3072 	#
3073         # trap.c sets stvec to point here, so
3074         # traps from user space start here,
3075         # in supervisor mode, but with a
3076         # user page table.
3077         #
3078 
3079         # save user a0 in sscratch so
3080         # a0 can be used to get at TRAPFRAME.
3081         csrw sscratch, a0
3082 
3083         # each process has a separate p->trapframe memory area,
3084         # but it's mapped to the same virtual address
3085         # (TRAPFRAME) in every process's user page table.
3086         li a0, TRAPFRAME
3087 
3088         # save the user registers in TRAPFRAME
3089         sd ra, 40(a0)
3090         sd sp, 48(a0)
3091         sd gp, 56(a0)
3092         sd tp, 64(a0)
3093         sd t0, 72(a0)
3094         sd t1, 80(a0)
3095         sd t2, 88(a0)
3096         sd s0, 96(a0)
3097         sd s1, 104(a0)
3098         sd a1, 120(a0)
3099         sd a2, 128(a0)
3100         sd a3, 136(a0)
3101         sd a4, 144(a0)
3102         sd a5, 152(a0)
3103         sd a6, 160(a0)
3104         sd a7, 168(a0)
3105         sd s2, 176(a0)
3106         sd s3, 184(a0)
3107         sd s4, 192(a0)
3108         sd s5, 200(a0)
3109         sd s6, 208(a0)
3110         sd s7, 216(a0)
3111         sd s8, 224(a0)
3112         sd s9, 232(a0)
3113         sd s10, 240(a0)
3114         sd s11, 248(a0)
3115         sd t3, 256(a0)
3116         sd t4, 264(a0)
3117         sd t5, 272(a0)
3118         sd t6, 280(a0)
3119 
3120 	# save the user a0 in p->trapframe->a0
3121         csrr t0, sscratch
3122         sd t0, 112(a0)
3123 
3124         # initialize kernel stack pointer, from p->trapframe->kernel_sp
3125         ld sp, 8(a0)
3126 
3127         # make tp hold the current hartid, from p->trapframe->kernel_hartid
3128         ld tp, 32(a0)
3129 
3130         # load the address of usertrap(), from p->trapframe->kernel_trap
3131         ld t0, 16(a0)
3132 
3133         # fetch the kernel page table address, from p->trapframe->kernel_satp.
3134         ld t1, 0(a0)
3135 
3136         # wait for any previous memory operations to complete, so that
3137         # they use the user page table.
3138         sfence.vma zero, zero
3139 
3140         # install the kernel page table.
3141         csrw satp, t1
3142 
3143         # flush now-stale user entries from the TLB.
3144         sfence.vma zero, zero
3145 
3146         # call usertrap()
3147         jalr t0
3148 
3149 
3150 .globl userret
3151 userret:
3152         # usertrap() returns here, with user satp in a0.
3153         # return from kernel to user.
3154 
3155         # switch to the user page table.
3156         sfence.vma zero, zero
3157         csrw satp, a0
3158         sfence.vma zero, zero
3159 
3160         li a0, TRAPFRAME
3161 
3162         # restore all but a0 from TRAPFRAME
3163         ld ra, 40(a0)
3164         ld sp, 48(a0)
3165         ld gp, 56(a0)
3166         ld tp, 64(a0)
3167         ld t0, 72(a0)
3168         ld t1, 80(a0)
3169         ld t2, 88(a0)
3170         ld s0, 96(a0)
3171         ld s1, 104(a0)
3172         ld a1, 120(a0)
3173         ld a2, 128(a0)
3174         ld a3, 136(a0)
3175         ld a4, 144(a0)
3176         ld a5, 152(a0)
3177         ld a6, 160(a0)
3178         ld a7, 168(a0)
3179         ld s2, 176(a0)
3180         ld s3, 184(a0)
3181         ld s4, 192(a0)
3182         ld s5, 200(a0)
3183         ld s6, 208(a0)
3184         ld s7, 216(a0)
3185         ld s8, 224(a0)
3186         ld s9, 232(a0)
3187         ld s10, 240(a0)
3188         ld s11, 248(a0)
3189         ld t3, 256(a0)
3190         ld t4, 264(a0)
3191         ld t5, 272(a0)
3192         ld t6, 280(a0)
3193 
3194 	# restore user a0
3195         ld a0, 112(a0)
3196 
3197         # return to user mode and user pc.
3198         # usertrapret() set up sstatus and sepc.
3199         sret

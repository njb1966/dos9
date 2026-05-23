3200         #
3201         # interrupts and exceptions while in supervisor
3202         # mode come here.
3203         #
3204         # the current stack is a kernel stack.
3205         # push registers, call kerneltrap().
3206         # when kerneltrap() returns, restore registers, return.
3207         #
3208 .globl kerneltrap
3209 .globl kernelvec
3210 .align 4
3211 kernelvec:
3212         # make room to save registers.
3213         addi sp, sp, -256
3214 
3215         # save caller-saved registers.
3216         sd ra, 0(sp)
3217         # sd sp, 8(sp)
3218         sd gp, 16(sp)
3219         sd tp, 24(sp)
3220         sd t0, 32(sp)
3221         sd t1, 40(sp)
3222         sd t2, 48(sp)
3223         sd a0, 72(sp)
3224         sd a1, 80(sp)
3225         sd a2, 88(sp)
3226         sd a3, 96(sp)
3227         sd a4, 104(sp)
3228         sd a5, 112(sp)
3229         sd a6, 120(sp)
3230         sd a7, 128(sp)
3231         sd t3, 216(sp)
3232         sd t4, 224(sp)
3233         sd t5, 232(sp)
3234         sd t6, 240(sp)
3235 
3236         # call the C trap handler in trap.c
3237         call kerneltrap
3238 
3239         # restore registers.
3240         ld ra, 0(sp)
3241         # ld sp, 8(sp)
3242         ld gp, 16(sp)
3243         # not tp (contains hartid), in case we moved CPUs
3244         ld t0, 32(sp)
3245         ld t1, 40(sp)
3246         ld t2, 48(sp)
3247         ld a0, 72(sp)
3248         ld a1, 80(sp)
3249         ld a2, 88(sp)
3250         ld a3, 96(sp)
3251         ld a4, 104(sp)
3252         ld a5, 112(sp)
3253         ld a6, 120(sp)
3254         ld a7, 128(sp)
3255         ld t3, 216(sp)
3256         ld t4, 224(sp)
3257         ld t5, 232(sp)
3258         ld t6, 240(sp)
3259 
3260         addi sp, sp, 256
3261 
3262         # return to whatever we were doing in the kernel.
3263         sret
3264 
3265 
3266 
3267 
3268 
3269 
3270 
3271 
3272 
3273 
3274 
3275 
3276 
3277 
3278 
3279 
3280 
3281 
3282 
3283 
3284 
3285 
3286 
3287 
3288 
3289 
3290 
3291 
3292 
3293 
3294 
3295 
3296 
3297 
3298 
3299 

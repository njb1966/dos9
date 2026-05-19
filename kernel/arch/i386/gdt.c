#include <gdt.h>
#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;   /* bits 0-15 of limit */
    uint16_t base_low;    /* bits 0-15 of base */
    uint8_t  base_mid;    /* bits 16-23 of base */
    uint8_t  access;      /* present, DPL, type flags */
    uint8_t  granularity; /* limit bits 16-19 + flags (G, D/B, L, AVL) */
    uint8_t  base_high;   /* bits 24-31 of base */
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;       /* size of GDT - 1 */
    uint32_t base;        /* linear address of GDT */
} __attribute__((packed));

#define GDT_ENTRIES 6

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdtp;

/* 32-bit TSS — only esp0/ss0 are strictly needed; rest zeroed. */
typedef struct {
    uint32_t prev;
    uint32_t esp0;   /* kernel stack pointer loaded on ring-3 → ring-0 transition */
    uint32_t ss0;    /* kernel stack segment */
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, iomap_base;
} __attribute__((packed)) tss_t;

static tss_t tss;

/* Defined in gdt_flush.S — loads GDT and reloads segment registers */
extern void gdt_flush(uint32_t gdtp_addr);

/*
 * access byte layout:
 *   bit 7   : present
 *   bits 6-5: DPL (ring 0-3)
 *   bit 4   : descriptor type (1 = code/data)
 *   bit 3   : executable
 *   bit 2   : direction/conforming
 *   bit 1   : readable (code) / writable (data)
 *   bit 0   : accessed (CPU sets this; leave 0)
 *
 * granularity byte layout:
 *   bit 7   : G (0 = byte granularity, 1 = 4KB page granularity)
 *   bit 6   : D/B (1 = 32-bit protected mode segment)
 *   bit 5   : L (1 = 64-bit code; 0 for us)
 *   bit 4   : AVL (available for OS use)
 *   bits 3-0: limit bits 16-19
 */
static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran)
{
    gdt[i].base_low   =  base        & 0xFFFF;
    gdt[i].base_mid   = (base >> 16) & 0xFF;
    gdt[i].base_high  = (base >> 24) & 0xFF;
    gdt[i].limit_low  =  limit       & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

void gdt_init(void)
{
    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint32_t)&gdt;

    /* 0: null descriptor — required; CPU uses selector 0 as "no segment" */
    gdt_set_entry(0, 0, 0,          0x00, 0x00);

    /* 1: kernel code — ring 0, executable, readable, flat 4GB */
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* 2: kernel data — ring 0, writable, flat 4GB */
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* 3: user code — ring 3, executable, readable, flat 4GB */
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    /* 4: user data — ring 3, writable, flat 4GB */
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    /* 5: TSS — type=0x89 (P=1, DPL=0, S=0, type=available 32-bit TSS) */
    tss.ss0        = SEG_KERNEL_DS;
    tss.iomap_base = sizeof(tss_t);   /* no I/O permission bitmap */
    gdt_set_entry(5, (uint32_t)&tss, sizeof(tss_t) - 1, 0x89, 0x00);

    gdt_flush((uint32_t)&gdtp);

    /* Load task register so the CPU can find the TSS on privilege changes. */
    __asm__ volatile("ltr %%ax" : : "a"((uint16_t)SEG_TSS));
}

void gdt_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}

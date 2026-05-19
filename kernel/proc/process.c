#include <process.h>
#include <pic.h>
#include <kheap.h>
#include <gdt.h>
#include <vmm.h>
#include <pmm.h>
#include <kernel.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define KSTACK_SIZE      8192u       /* 8KB kernel stack per process */
#define TIMESLICE        5u          /* timer ticks per scheduling quantum */
#define USER_STACK_TOP   0xBFFF0000u /* user-space stack pointer (grows down) */
#define USER_STACK_PAGES 4u          /* 16KB user stack (4 × 4KB pages) */
#define PAGE_SIZE        4096u

/* Defined in switch.S */
extern void context_switch(process_t *prev, process_t *next);
extern void enter_userspace(uint32_t user_eip, uint32_t user_esp);

static process_t *proc_table[MAX_PROCS];
static int        n_procs     = 0;
static int        cur         = 0;
static uint32_t   next_pid    = 0;
static uint32_t   ticks       = 0;

static uint32_t get_cr3(void) {
    uint32_t v;
    __asm__ volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

static void name_copy(char *dst, const char *src) {
    int i;
    for (i = 0; i < 15 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/*
 * Entry trampoline for newly created processes.
 *
 * Called via the fake 'ret' address placed on the initial kernel stack by
 * process_create().  Reads entry and self-pointer before enabling IRQs to
 * avoid a race where a reschedule fires and changes cur before we do.
 */
static void proc_trampoline(void) {
    process_t *self        = proc_table[cur];
    void      (*fn)(void)  = self->entry;

    __asm__ volatile("sti");    /* re-enable IRQs — CPU cleared IF on interrupt entry */
    fn();

    self->state = PROC_DEAD;
    for (;;) schedule();        /* yield forever if entry returns */
}

/* Entry trampoline for user-space processes.
   Runs in kernel mode; sets TSS kernel stack, then iret to ring 3. */
static void user_trampoline(void) {
    process_t *self = proc_table[cur];
    gdt_set_kernel_stack((uint32_t)(self->stack + KSTACK_SIZE));
    enter_userspace((uint32_t)(uintptr_t)self->entry, self->user_stack);
}

static void timer_tick(struct registers *r) {
    (void)r;
    if (++ticks % TIMESLICE == 0)
        schedule();
}

/* ── public API ─────────────────────────────────────────────────────── */

void process_init(void) {
    process_t *p = kmalloc(sizeof(process_t));
    p->esp      = 0;            /* set on first context_switch away */
    p->page_dir = get_cr3();
    p->pid      = next_pid++;
    p->state    = PROC_RUNNING;
    name_copy(p->name, "init");
    p->entry    = NULL;
    p->stack    = NULL;         /* boot stack — not heap-allocated */

    proc_table[0] = p;
    n_procs       = 1;
    cur           = 0;

    irq_register(IRQ_TIMER, timer_tick);
}

/* Recycle a dead slot or claim the next free one.  Frees the dead
   process's kernel stack and struct; user page directories are not
   freed here (no PMM walk API yet — known leak). */
static int proc_alloc_slot(void) {
    for (int i = 0; i < n_procs; i++) {
        process_t *old = proc_table[i];
        if (old && old->state == PROC_DEAD) {
            if (old->stack) kfree(old->stack);
            kfree(old);
            proc_table[i] = NULL;
            return i;
        }
    }
    if (n_procs >= MAX_PROCS) return -1;
    return n_procs++;
}

process_t *process_create(void (*entry)(void), const char *name) {
    int slot = proc_alloc_slot();
    if (slot < 0) return NULL;

    process_t *p   = kmalloc(sizeof(process_t));
    uint8_t   *stk = kmalloc(KSTACK_SIZE);

    /*
     * Build a fake context_switch frame on the new stack.
     * context_switch restores EBX, ESI, EDI, EBP (all 0 here) then
     * 'ret's to proc_trampoline.
     *
     * Stack layout (high→low address, push order):
     *   proc_trampoline  ← [top-4]  — the 'ret' target
     *   0 (EBP)          ← [top-8]
     *   0 (EDI)          ← [top-12]
     *   0 (ESI)          ← [top-16]
     *   0 (EBX)          ← [top-20]  ← new_proc->esp points here
     */
    uint32_t *top = (uint32_t *)(stk + KSTACK_SIZE);
    *--top = (uint32_t)proc_trampoline;
    *--top = 0;     /* EBP */
    *--top = 0;     /* EDI */
    *--top = 0;     /* ESI */
    *--top = 0;     /* EBX */

    p->esp      = (uint32_t)top;
    p->page_dir = get_cr3();
    p->pid      = next_pid++;
    p->state    = PROC_READY;
    name_copy(p->name, name);
    p->entry    = entry;
    p->stack    = stk;

    proc_table[slot] = p;
    return p;
}

void process_exit(void) {
    proc_table[cur]->state = PROC_DEAD;
    for (;;) schedule();
}

int process_kill(uint32_t pid) {
    if (pid == 0) return -1;                /* never kill init */
    for (int i = 0; i < n_procs; i++) {
        process_t *p = proc_table[i];
        if (!p || p->pid != pid) continue;
        if (p->state == PROC_DEAD) return 0;
        p->state = PROC_DEAD;
        if (i == cur) {                     /* killed self → never return */
            for (;;) schedule();
        }
        return 0;
    }
    return -1;
}

int process_count(void) { return n_procs; }

process_t *process_get(int idx) {
    if (idx < 0 || idx >= n_procs) return NULL;
    return proc_table[idx];
}

void schedule(void) {
    if (n_procs <= 1) return;

    int next = cur;
    for (int i = 1; i <= n_procs; i++) {
        int idx = (cur + i) % n_procs;
        if (proc_table[idx] && proc_table[idx]->state == PROC_READY) {
            next = idx;
            break;
        }
    }
    if (next == cur) return;

    process_t *prev = proc_table[cur];
    process_t *nxt  = proc_table[next];
    if (prev->state != PROC_DEAD) prev->state = PROC_READY;
    nxt->state  = PROC_RUNNING;
    cur         = next;
    /* Keep TSS.esp0 pointing at the new process's kernel stack top so that
       interrupts and syscalls from ring 3 switch to the right kernel stack. */
    if (nxt->stack)
        gdt_set_kernel_stack((uint32_t)(nxt->stack + KSTACK_SIZE));
    context_switch(prev, nxt);
}

process_t *process_create_user(uint32_t entry_vaddr, const char *name,
                                uint32_t pd_phys) {
    int slot = proc_alloc_slot();
    if (slot < 0) return NULL;

    process_t *p   = kmalloc(sizeof(process_t));
    uint8_t   *stk = kmalloc(KSTACK_SIZE);

    /* Allocate and map user-space stack pages in the user page directory. */
    for (uint32_t i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t vaddr = USER_STACK_TOP - (USER_STACK_PAGES - i) * PAGE_SIZE;
        uint32_t frame = (uint32_t)pmm_alloc_frame();
        memset((void *)VIRT(frame), 0, PAGE_SIZE);
        vmm_map_page_in(pd_phys, vaddr, frame,
                        PF_PRESENT | PF_WRITE | PF_USER);
    }

    /* Build kernel stack fake frame: ret → user_trampoline. */
    uint32_t *top = (uint32_t *)(stk + KSTACK_SIZE);
    *--top = (uint32_t)user_trampoline;
    *--top = 0;  /* EBP */
    *--top = 0;  /* EDI */
    *--top = 0;  /* ESI */
    *--top = 0;  /* EBX */

    p->esp        = (uint32_t)top;
    p->page_dir   = pd_phys;
    p->pid        = next_pid++;
    p->state      = PROC_READY;
    name_copy(p->name, name);
    p->entry      = (void (*)(void))(uintptr_t)entry_vaddr;
    p->stack      = stk;
    p->user_stack = USER_STACK_TOP;

    proc_table[slot] = p;
    return p;
}

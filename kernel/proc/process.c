#include <process.h>
#include <pic.h>
#include <kheap.h>
#include <stdint.h>
#include <stddef.h>

#define KSTACK_SIZE 8192u   /* 8KB kernel stack per process */
#define TIMESLICE   5u      /* timer ticks per scheduling quantum */

/* Defined in switch.S */
extern void context_switch(process_t *prev, process_t *next);

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

process_t *process_create(void (*entry)(void), const char *name) {
    if (n_procs >= MAX_PROCS) return NULL;

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

    proc_table[n_procs++] = p;
    return p;
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
    prev->state = PROC_READY;
    nxt->state  = PROC_RUNNING;
    cur         = next;
    context_switch(prev, nxt);
}

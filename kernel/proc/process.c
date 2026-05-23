#include <process.h>
#include <vfs.h>
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
#define MAX_USER_ARGC    16

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

static int name_copy(char *dst, const char *src) {
    int i;
    if (!src || !*src) return -1;
    for (i = 0; i < 15 && src[i]; i++) dst[i] = src[i];
    if (src[i]) return -1;
    dst[i] = '\0';
    return 0;
}

static uint32_t alloc_pid(void) {
    for (;;) {
        uint32_t pid = next_pid++;
        if (pid == 0) continue;
        int in_use = 0;
        for (int i = 0; i < n_procs; i++) {
            process_t *p = proc_table[i];
            if (p && p->pid == pid && p->state != PROC_DEAD) {
                in_use = 1;
                break;
            }
        }
        if (!in_use) return pid;
    }
}

/*
 * Tear down a user page directory created for a soon-to-be-abandoned
 * process.  The kernel half is shared; only the user mappings are owned
 * by this page directory.
 */
static void destroy_user_pd(uint32_t pd_phys) {
    uint32_t *pd = (uint32_t *)VIRT(pd_phys);

    for (uint32_t pd_idx = 0; pd_idx < 768u; pd_idx++) {
        if (!(pd[pd_idx] & PF_PRESENT)) continue;

        uint32_t *pt = (uint32_t *)VIRT(pd[pd_idx] & ~0xFFFu);
        for (uint32_t pt_idx = 0; pt_idx < 1024u; pt_idx++) {
            if (!(pt[pt_idx] & PF_PRESENT)) continue;
            pmm_free_frame((void *)(uintptr_t)(pt[pt_idx] & ~0xFFFu));
        }

        pmm_free_frame((void *)(uintptr_t)(pd[pd_idx] & ~0xFFFu));
        pd[pd_idx] = 0;
    }

    pmm_free_frame((void *)(uintptr_t)pd_phys);
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

    self->exit_code = 0;
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
    p->pid      = alloc_pid();
    p->state    = PROC_RUNNING;
    name_copy(p->name, "init");
    p->entry    = NULL;
    p->stack    = NULL;         /* boot stack — not heap-allocated */
    memset(p->fds, 0, sizeof(p->fds));

    proc_table[0] = p;
    n_procs       = 1;
    cur           = 0;

    irq_register(IRQ_TIMER, timer_tick);
}

/* Recycle a dead slot or claim the next free one.
   `new_slot_out` is set when a fresh table entry was reserved. */
static int proc_alloc_slot(int *new_slot_out) {
    if (new_slot_out) *new_slot_out = 0;
    for (int i = 0; i < n_procs; i++) {
        process_t *old = proc_table[i];
        if (old && old->state == PROC_DEAD) {
            vfs_close_table(old->fds);   /* handles kill path that skips process_exit */
            if (old->brk != 0)
                destroy_user_pd(old->page_dir);
            if (old->stack) kfree(old->stack);
            kfree(old);
            proc_table[i] = NULL;
            return i;
        }
    }
    if (n_procs >= MAX_PROCS) return -1;
    if (new_slot_out) *new_slot_out = 1;
    return n_procs++;
}

process_t *process_create(void (*entry)(void), const char *name) {
    int fresh = 0;
    int slot = proc_alloc_slot(&fresh);
    if (slot < 0) return NULL;

    process_t *p   = kmalloc(sizeof(process_t));
    uint8_t   *stk = kmalloc(KSTACK_SIZE);
    if (!p || !stk) {
        if (stk) kfree(stk);
        if (p) kfree(p);
        if (fresh && n_procs > 0 && slot == n_procs - 1) n_procs--;
        return NULL;
    }

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
    p->pid      = alloc_pid();
    p->state     = PROC_READY;
    if (name_copy(p->name, name) < 0) {
        kfree(stk);
        kfree(p);
        if (fresh && n_procs > 0 && slot == n_procs - 1) n_procs--;
        return NULL;
    }
    p->entry     = entry;
    p->stack     = stk;
    p->brk       = 0;
    p->exit_code = 0;
    memset(p->fds, 0, sizeof(p->fds));

    proc_table[slot] = p;
    return p;
}

void process_exit(int32_t code) {
    vfs_close_table(proc_table[cur]->fds);
    proc_table[cur]->exit_code = code;
    proc_table[cur]->state = PROC_DEAD;
    /* Wake any process blocked in sys_waitpid so it can reap this child. */
    for (int i = 0; i < n_procs; i++) {
        if (proc_table[i] && proc_table[i]->state == PROC_BLOCKED)
            proc_table[i]->state = PROC_READY;
    }
    for (;;) schedule();
}

int process_kill(uint32_t pid) {
    if (pid == 0) return -1;                /* never kill init */
    for (int i = 0; i < n_procs; i++) {
        process_t *p = proc_table[i];
        if (!p || p->pid != pid) continue;
        if (p->state == PROC_DEAD) return 0;
        p->state = PROC_DEAD;
        for (int j = 0; j < n_procs; j++) {
            if (proc_table[j] && proc_table[j]->state == PROC_BLOCKED)
                proc_table[j]->state = PROC_READY;
        }
        if (i == cur) {                     /* killed self → never return */
            for (;;) schedule();
        }
        return 0;
    }
    return -1;
}

int process_count(void) { return n_procs; }

uint32_t process_ticks(void) { return ticks; }

process_t *process_get(int idx) {
    if (idx < 0 || idx >= n_procs) return NULL;
    return proc_table[idx];
}

void schedule(void) {
    if (n_procs <= 1) {
        /* Single process — sleep until the next interrupt so device I/O
           and timers can make progress without busy-spinning. */
        __asm__ volatile("sti; hlt");
        return;
    }

    int next = cur;
    for (int i = 1; i <= n_procs; i++) {
        int idx = (cur + i) % n_procs;
        if (proc_table[idx] && proc_table[idx]->state == PROC_READY) {
            next = idx;
            break;
        }
    }
    if (next == cur) {
        /* No other runnable process — enable IRQs and wait for the next
           interrupt before returning to caller. */
        __asm__ volatile("sti; hlt");
        return;
    }

    process_t *prev = proc_table[cur];
    process_t *nxt  = proc_table[next];
    /* Only PROC_RUNNING → PROC_READY; preserve PROC_BLOCKED set by waitpid. */
    if (prev->state == PROC_RUNNING) prev->state = PROC_READY;
    nxt->state  = PROC_RUNNING;
    cur         = next;
    /* Keep TSS.esp0 pointing at the new process's kernel stack top so that
       interrupts and syscalls from ring 3 switch to the right kernel stack. */
    if (nxt->stack)
        gdt_set_kernel_stack((uint32_t)(nxt->stack + KSTACK_SIZE));
    context_switch(prev, nxt);
}

/*
 * write_user_argv — write argc/argv to the topmost user stack page.
 *
 * frame_data: kernel pointer to the physical frame of the topmost stack page
 *             (user virtual base: USER_STACK_TOP - PAGE_SIZE = 0xBFFE0000)
 * Returns the user-space virtual address of the argc slot (new esp),
 * or 0 on failure.
 */
static uint32_t write_user_argv(uint8_t *frame_data,
                                 const char **argv, int argc) {
    if (!frame_data || argc <= 0 || !argv) return 0;
    if (argc > MAX_USER_ARGC) return 0;

    uint32_t off = PAGE_SIZE;  /* grows downward */

    uint32_t str_off[MAX_USER_ARGC];
    uint32_t page_ubase = USER_STACK_TOP - PAGE_SIZE;  /* 0xBFFE0000 */

    /* Write strings high→low (last arg first). */
    for (int i = argc - 1; i >= 0; i--) {
        const char *s = (argv[i] && argv[i][0]) ? argv[i] : "";
        uint32_t len = 0;
        while (s[len]) len++;
        len++;                      /* include NUL */
        if (len > 128) return 0;
        if (off < len) return 0;
        off -= len;
        for (uint32_t j = 0; j < len - 1; j++)
            frame_data[off + j] = (uint8_t)s[j];
        frame_data[off + len - 1] = 0;
        str_off[i] = off;
    }

    off &= ~3u;  /* align to 4 bytes */

    /* NULL sentinel. */
    if (off < 4) return 0;
    off -= 4;
    *(uint32_t *)(frame_data + off) = 0;

    /* argv pointer array (last → first, so argv[0] lands lowest). */
    for (int i = argc - 1; i >= 0; i--) {
        if (off < 4) return 0;
        off -= 4;
        *(uint32_t *)(frame_data + off) = page_ubase + str_off[i];
    }

    /* argc. */
    if (off < 4) return 0;
    off -= 4;
    *(uint32_t *)(frame_data + off) = (uint32_t)argc;

    return page_ubase + off;
}

process_t *process_create_user(uint32_t entry_vaddr, const char *name,
                                uint32_t pd_phys,
                                const char **argv, int argc) {
    int fresh = 0;
    int slot = proc_alloc_slot(&fresh);
    if (slot < 0) return NULL;

    process_t *p   = kmalloc(sizeof(process_t));
    uint8_t   *stk = kmalloc(KSTACK_SIZE);
    if (!p || !stk) {
        if (stk) kfree(stk);
        if (p) kfree(p);
        if (fresh && n_procs > 0 && slot == n_procs - 1) n_procs--;
        return NULL;
    }

    /* Allocate and map user-space stack pages; save topmost frame pointer. */
    uint8_t *top_frame_kptr = NULL;
    for (uint32_t i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t vaddr = USER_STACK_TOP - (USER_STACK_PAGES - i) * PAGE_SIZE;
        uint32_t frame = (uint32_t)pmm_alloc_frame();
        if (!frame) {
            destroy_user_pd(pd_phys);
            kfree(stk);
            kfree(p);
            if (fresh && n_procs > 0 && slot == n_procs - 1) n_procs--;
            return NULL;
        }
        memset((void *)VIRT(frame), 0, PAGE_SIZE);
        if (vmm_map_page_in(pd_phys, vaddr, frame,
                            PF_PRESENT | PF_WRITE | PF_USER) < 0) {
            pmm_free_frame((void *)(uintptr_t)frame);
            destroy_user_pd(pd_phys);
            kfree(stk);
            kfree(p);
            if (fresh && n_procs > 0 && slot == n_procs - 1) n_procs--;
            return NULL;
        }
        if (i == USER_STACK_PAGES - 1)
            top_frame_kptr = (uint8_t *)VIRT(frame);
    }

    /* Always set up at least argc=1, argv[0]=name. */
    uint32_t user_esp = USER_STACK_TOP;
    if (top_frame_kptr) {
        const char *synth_argv[1];
        int synth_argc = argc;
        const char **use_argv = argv;
        if (synth_argc <= 0 || !use_argv) {
            synth_argv[0] = name;
            use_argv = synth_argv;
            synth_argc = 1;
        }
        uint32_t esp = write_user_argv(top_frame_kptr, use_argv, synth_argc);
        if (esp) user_esp = esp;
        else {
            destroy_user_pd(pd_phys);
            kfree(stk);
            kfree(p);
            if (fresh && n_procs > 0 && slot == n_procs - 1) n_procs--;
            return NULL;
        }
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
    p->state      = PROC_READY;
    if (name_copy(p->name, name) < 0) {
        destroy_user_pd(pd_phys);
        kfree(stk);
        kfree(p);
        if (fresh && n_procs > 0 && slot == n_procs - 1) n_procs--;
        return NULL;
    }
    p->pid        = alloc_pid();
    p->entry      = (void (*)(void))(uintptr_t)entry_vaddr;
    p->stack      = stk;
    p->user_stack = user_esp;
    p->brk        = 0;
    p->exit_code  = 0;
    memset(p->fds, 0, sizeof(p->fds));
    vfs_inherit_stdio(proc_table[cur]->fds, p->fds);

    proc_table[slot] = p;
    return p;
}

uint32_t process_getpid(void) {
    return proc_table[cur]->pid;
}

file_t *process_current_fds(void) {
    return proc_table[cur]->fds;
}

process_t *process_current(void) {
    return proc_table[cur];
}

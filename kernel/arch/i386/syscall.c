#include <syscall.h>
#include <idt.h>
#include <vfs.h>
#include <process.h>
#include <vmm.h>
#include <pmm.h>
#include <kernel.h>
#include <string.h>
#include <stdint.h>

extern void isr128(void);

#define PAGE_SIZE    4096u
#define USER_BRK_MAX 0xBF000000u   /* ceiling: leaves room below the user stack */

/* ── syscall implementations ─────────────────────────────────────────── */

static int32_t sys_exit(int32_t code) {
    (void)code;
    process_exit();
    __builtin_unreachable();
}

static int32_t sys_read(int32_t fd, void *buf, uint32_t len) {
    return vfs_read(fd, buf, len);
}

static int32_t sys_write(int32_t fd, const void *buf, uint32_t len) {
    return vfs_write(fd, buf, len);
}

static int32_t sys_open(const char *path, int32_t flags) {
    return vfs_open(path, flags);
}

static int32_t sys_close(int32_t fd) {
    return vfs_close(fd);
}

static int32_t sys_lseek(int32_t fd, int32_t offset, int32_t whence) {
    return vfs_lseek(fd, offset, (int)whence);
}

static int32_t sys_getpid(void) {
    return (int32_t)process_getpid();
}

/*
 * sys_brk — sbrk() semantics.
 *
 * increment == 0 : return current break (query)
 * increment  > 0 : extend heap by increment bytes; return old break
 * increment  < 0 : not supported; return -1
 *
 * New pages are zeroed and mapped user-writable in the calling process's
 * page directory.
 */
static int32_t sys_brk(int32_t increment) {
    process_t *p = process_current();

    if (p->brk == 0)
        return -1;   /* kernel thread or brk not initialised */

    if (increment == 0)
        return (int32_t)p->brk;

    if (increment < 0)
        return -1;   /* shrink not supported yet */

    uint32_t old_brk = p->brk;
    uint32_t new_brk = old_brk + (uint32_t)increment;

    if (new_brk > USER_BRK_MAX || new_brk < old_brk)   /* overflow / ceiling */
        return -1;

    /* Map any pages that the extension crosses into. */
    uint32_t old_top = (old_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1u);
    uint32_t new_top = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1u);

    for (uint32_t va = old_top; va < new_top; va += PAGE_SIZE) {
        uint32_t frame = (uint32_t)pmm_alloc_frame();
        if (!frame) return -1;
        memset((void *)VIRT(frame), 0, PAGE_SIZE);
        vmm_map_page_in(p->page_dir, va, frame,
                        PF_PRESENT | PF_WRITE | PF_USER);
    }

    p->brk = new_brk;
    return (int32_t)old_brk;   /* caller's allocation starts at old_brk */
}

/* ── dispatcher ──────────────────────────────────────────────────────── */

void syscall_handler(struct registers *r) {
    int32_t ret = -1;
    switch ((int32_t)r->eax) {
    case SYS_EXIT:   sys_exit  ((int32_t)r->ebx);                               return;
    case SYS_READ:   ret = sys_read  ((int32_t)r->ebx, (void *)r->ecx, r->edx); break;
    case SYS_WRITE:  ret = sys_write ((int32_t)r->ebx, (void *)r->ecx, r->edx); break;
    case SYS_OPEN:   ret = sys_open  ((const char *)r->ebx, (int32_t)r->ecx);   break;
    case SYS_CLOSE:  ret = sys_close ((int32_t)r->ebx);                          break;
    case SYS_LSEEK:  ret = sys_lseek ((int32_t)r->ebx, (int32_t)r->ecx,
                                      (int32_t)r->edx);                          break;
    case SYS_GETPID: ret = sys_getpid();                                          break;
    case SYS_BRK:    ret = sys_brk   ((int32_t)r->ebx);                          break;
    }
    r->eax = (uint32_t)ret;
}

void syscall_init(void) {
    idt_set_gate(0x80, isr128, INT_GATE_U);
}

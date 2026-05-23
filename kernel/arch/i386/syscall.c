#include <syscall.h>
#include <idt.h>
#include <vfs.h>
#include <process.h>
#include <vmm.h>
#include <pmm.h>
#include <kernel.h>
#include <string.h>
#include <stdint.h>
#include <kheap.h>
#include <elf.h>
#include <pipe.h>

extern void isr128(void);

#define PAGE_SIZE    4096u
#define USER_BRK_MAX 0xBF000000u   /* ceiling: leaves room below the user stack */

static void user_unmap_range(uint32_t pd_phys, uint32_t start, uint32_t end) {
    if (start >= end) return;
    uint32_t *pd = (uint32_t *)VIRT(pd_phys);

    for (uint32_t va = start; va < end; va += PAGE_SIZE) {
        uint32_t pd_idx = va >> 22;
        uint32_t pt_idx = (va >> 12) & 0x3FFu;
        if (!(pd[pd_idx] & PF_PRESENT)) continue;

        uint32_t *pt = (uint32_t *)VIRT(pd[pd_idx] & ~0xFFFu);
        if (!(pt[pt_idx] & PF_PRESENT)) continue;
        pmm_free_frame((void *)(uintptr_t)(pt[pt_idx] & ~0xFFFu));
        pt[pt_idx] = 0;
    }

    for (uint32_t pd_idx = start >> 22; pd_idx <= ((end - 1u) >> 22); pd_idx++) {
        if (!(pd[pd_idx] & PF_PRESENT)) continue;
        uint32_t *pt = (uint32_t *)VIRT(pd[pd_idx] & ~0xFFFu);
        int empty = 1;
        for (uint32_t i = 0; i < 1024u; i++) {
            if (pt[i] & PF_PRESENT) {
                empty = 0;
                break;
            }
        }
        if (!empty) continue;
        pmm_free_frame((void *)(uintptr_t)(pd[pd_idx] & ~0xFFFu));
        pd[pd_idx] = 0;
    }
}

/* ── syscall implementations ─────────────────────────────────────────── */

static int32_t sys_exit(int32_t code) {
    process_exit(code);
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
        if (!frame) {
            user_unmap_range(p->page_dir, old_top, va);
            return -1;
        }
        memset((void *)VIRT(frame), 0, PAGE_SIZE);
        if (vmm_map_page_in(p->page_dir, va, frame,
                            PF_PRESENT | PF_WRITE | PF_USER) < 0) {
            pmm_free_frame((void *)(uintptr_t)frame);
            user_unmap_range(p->page_dir, old_top, va);
            return -1;
        }
    }

    p->brk = new_brk;
    return (int32_t)old_brk;   /* caller's allocation starts at old_brk */
}

/*
 * sys_exec — load and launch a user ELF from a path.
 * argv: user-space pointer array (ecx), argc: count (edx).
 * Returns the new PID on success, -1 on failure.
 */
static int32_t sys_exec(const char *path, const char **argv, int32_t argc) {
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) return -1;

    /* Determine file size via lseek. */
    int32_t size = vfs_lseek(fd, 0, SEEK_END);
    if (size <= 0) { vfs_close(fd); return -1; }
    vfs_lseek(fd, 0, SEEK_SET);

    void *buf = kmalloc((uint32_t)size);
    if (!buf) { vfs_close(fd); return -1; }

    int n = vfs_read(fd, buf, (uint32_t)size);
    vfs_close(fd);
    if (n != size) { kfree(buf); return -1; }

    uint32_t pd_phys = 0, brk_start = 0;
    uint32_t entry = elf_load(buf, (uint32_t)size, &pd_phys, &brk_start);
    kfree(buf);
    if (!entry || !pd_phys) return -1;

    /* Derive a short name from the path (basename). */
    const char *name = path;
    for (const char *p = path; *p; p++)
        if (*p == '/') name = p + 1;

    process_t *p = process_create_user(entry, name, pd_phys, argv, (int)argc);
    if (!p) return -1;
    p->brk = brk_start;
    return (int32_t)p->pid;
}

/* sys_readdir — thin wrapper: fd + index → name (idx in edx, buf in ecx, nmax in esi). */
static int32_t sys_readdir(int32_t fd, char *buf, uint32_t nmax, uint32_t idx) {
    return vfs_readdir(fd, idx, buf, nmax);
}

/* sys_unlink — remove a name from its parent directory. */
static int32_t sys_unlink(const char *path) {
    return vfs_unlink(path);
}

/* sys_pipe — create an anonymous pipe; fds_user[0]=read, fds_user[1]=write. */
static int32_t sys_pipe(int32_t *fds_user) {
    int fds[2];
    if (pipe_create(fds) < 0) return -1;
    fds_user[0] = fds[0];
    fds_user[1] = fds[1];
    return 0;
}

/* sys_dup — duplicate fd to next free slot. */
static int32_t sys_dup(int32_t oldfd) {
    return (int32_t)vfs_dup((int)oldfd);
}

/* sys_dup2 — duplicate oldfd to newfd. */
static int32_t sys_dup2(int32_t oldfd, int32_t newfd) {
    return (int32_t)vfs_dup2((int)oldfd, (int)newfd);
}

/*
 * sys_waitpid — block until the target process is PROC_DEAD or gone.
 * Returns 0 on success, -1 if pid never existed.
 */
static int32_t sys_waitpid(int32_t pid) {
    int found = 0;
    for (;;) {
        found = 0;
        for (int i = 0; i < MAX_PROCS; i++) {
            process_t *p = process_get(i);
            if (!p) continue;
            if ((int32_t)p->pid == pid) {
                found = 1;
                if (p->state == PROC_DEAD) return p->exit_code;
                break;
            }
        }
        if (!found) return 0;   /* already gone — treat as success */
        process_current()->state = PROC_BLOCKED;
        schedule();
    }
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
    case SYS_BRK:     ret = sys_brk    ((int32_t)r->ebx);                          break;
    case SYS_EXEC:    ret = sys_exec   ((const char *)r->ebx, (const char **)r->ecx, (int32_t)r->edx); break;
    case SYS_READDIR: ret = sys_readdir((int32_t)r->ebx, (char *)r->ecx,
                                        r->edx, r->esi);                          break;
    case SYS_UNLINK:  ret = sys_unlink ((const char *)r->ebx);                    break;
    case SYS_WAITPID: ret = sys_waitpid((int32_t)r->ebx);                         break;
    case SYS_PIPE:    ret = sys_pipe   ((int32_t *)r->ebx);                        break;
    case SYS_DUP:     ret = sys_dup    ((int32_t)r->ebx);                          break;
    case SYS_DUP2:    ret = sys_dup2   ((int32_t)r->ebx, (int32_t)r->ecx);        break;
    }
    r->eax = (uint32_t)ret;
}

void syscall_init(void) {
    idt_set_gate(0x80, isr128, INT_GATE_U);
}

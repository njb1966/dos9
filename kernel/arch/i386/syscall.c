#include <syscall.h>
#include <idt.h>
#include <vfs.h>
#include <process.h>
#include <stdint.h>

extern void isr128(void);

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

/* ── dispatcher ──────────────────────────────────────────────────────── */

/* Called from syscall_common in isr.S.  Writes return value to r->eax
   so popa restores it into the caller's EAX register. */
void syscall_handler(struct registers *r) {
    int32_t ret = -1;
    switch ((int32_t)r->eax) {
    case SYS_EXIT:  sys_exit((int32_t)r->ebx);                                 return;
    case SYS_READ:  ret = sys_read ((int32_t)r->ebx, (void *)r->ecx, r->edx); break;
    case SYS_WRITE: ret = sys_write((int32_t)r->ebx, (void *)r->ecx, r->edx); break;
    case SYS_OPEN:  ret = sys_open ((const char *)r->ebx, (int32_t)r->ecx);   break;
    case SYS_CLOSE: ret = sys_close((int32_t)r->ebx);                          break;
    }
    r->eax = (uint32_t)ret;
}

void syscall_init(void) {
    idt_set_gate(0x80, isr128, INT_GATE_U);
}

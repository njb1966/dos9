#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* ── Syscall numbers ──────────────────────────────────────────────────── */
#define SYS_EXIT   0
#define SYS_READ   1
#define SYS_WRITE  2
#define SYS_OPEN   3
#define SYS_CLOSE  4

/* ── File descriptor constants ────────────────────────────────────────── */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* ── Open flags ───────────────────────────────────────────────────────── */
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2

/* ── Raw syscall (eax=number, ebx/ecx/edx=args, return in eax) ────────── */
static inline int32_t _syscall3(int32_t n, int32_t a, int32_t b, int32_t c) {
    int32_t ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(n), "b"(a), "c"(b), "d"(c)
        : "memory");
    return ret;
}
#define _syscall2(n,a,b) _syscall3((n),(a),(b),0)
#define _syscall1(n,a)   _syscall3((n),(a),0,0)

/* ── Syscall wrappers ─────────────────────────────────────────────────── */
static inline void _exit(int code) {
    _syscall1(SYS_EXIT, (int32_t)code);
    __builtin_unreachable();
}

static inline int write(int fd, const void *buf, size_t n) {
    return (int)_syscall3(SYS_WRITE, (int32_t)fd,
                          (int32_t)(uintptr_t)buf, (int32_t)n);
}

static inline int read(int fd, void *buf, size_t n) {
    return (int)_syscall3(SYS_READ, (int32_t)fd,
                          (int32_t)(uintptr_t)buf, (int32_t)n);
}

static inline int open(const char *path, int flags) {
    return (int)_syscall2(SYS_OPEN, (int32_t)(uintptr_t)path, (int32_t)flags);
}

static inline int close(int fd) {
    return (int)_syscall1(SYS_CLOSE, (int32_t)fd);
}

/* ── stdio ──────────────────────────────────────────────────────────────── */
void putchar(int c);
int  puts(const char *s);
int  vprintf(const char *fmt, va_list ap);
int  printf(const char *fmt, ...);

/* ── string ─────────────────────────────────────────────────────────────── */
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
void  *memset(void *dst, int c, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);

/* ── stdlib ─────────────────────────────────────────────────────────────── */
void exit(int code);

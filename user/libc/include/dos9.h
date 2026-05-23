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
#define SYS_LSEEK  5
#define SYS_GETPID 6
#define SYS_BRK     7
#define SYS_EXEC    8
#define SYS_READDIR 9
#define SYS_UNLINK  10
#define SYS_WAITPID 11
#define SYS_PIPE    12
#define SYS_DUP     13
#define SYS_DUP2    14

/* ── File descriptor constants ────────────────────────────────────────── */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* ── Open flags ───────────────────────────────────────────────────────── */
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  4
#define O_TRUNC  8

/* ── lseek whence ─────────────────────────────────────────────────────── */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* ── Raw syscall (eax=number, ebx/ecx/edx=args, return in eax) ────────── */
static inline int32_t _syscall3(int32_t n, int32_t a, int32_t b, int32_t c) {
    int32_t ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(n), "b"(a), "c"(b), "d"(c)
        : "memory");
    return ret;
}
/* _syscall4: uses esi (register "S") for the 4th argument. */
static inline int32_t _syscall4(int32_t n, int32_t a, int32_t b, int32_t c, int32_t d) {
    int32_t ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d)
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

static inline int lseek(int fd, int32_t offset, int whence) {
    return (int)_syscall3(SYS_LSEEK, (int32_t)fd, offset, (int32_t)whence);
}

static inline int getpid(void) {
    return (int)_syscall3(SYS_GETPID, 0, 0, 0);
}

/* sbrk — extend the heap by `increment` bytes; returns old break.
   Pass 0 to query the current break without moving it. */
static inline void *sbrk(int32_t increment) {
    int32_t ret = _syscall1(SYS_BRK, increment);
    return (void *)(int32_t)ret;   /* (void*)-1 on error */
}

/* exec — spawn a new user process from path; returns new pid, -1 on error. */
static inline int exec(const char *path) {
    return (int)_syscall3(SYS_EXEC, (int32_t)(uintptr_t)path, 0, 0);
}

/* execv — spawn process with argv; argv[0..argc-1] are the arguments,
   argv[argc] must be NULL.  Returns new pid, -1 on error. */
static inline int execv(const char *path, const char **argv, int argc) {
    return (int)_syscall3(SYS_EXEC, (int32_t)(uintptr_t)path,
                          (int32_t)(uintptr_t)argv, (int32_t)argc);
}

/* readdir — read directory entry at idx; returns 1 if found, 0 at end, -1 error. */
static inline int readdir(int fd, uint32_t idx, char *buf, uint32_t nmax) {
    return (int)_syscall4(SYS_READDIR, (int32_t)fd,
                          (int32_t)(uintptr_t)buf, (int32_t)nmax,
                          (int32_t)idx);
}

/* unlink — remove a file by path; returns 0 on success, -1 on error. */
static inline int unlink(const char *path) {
    return (int)_syscall1(SYS_UNLINK, (int32_t)(uintptr_t)path);
}

/* waitpid — block until pid exits; returns child's exit code, -1 on error. */
static inline int waitpid(int pid) {
    return (int)_syscall1(SYS_WAITPID, (int32_t)pid);
}

/* pipe — create anonymous pipe; fds[0]=read end, fds[1]=write end. */
static inline int pipe(int fds[2]) {
    return (int)_syscall1(SYS_PIPE, (int32_t)(uintptr_t)fds);
}

/* dup — duplicate fd to next free slot; returns new fd, -1 on error. */
static inline int dup(int fd) {
    return (int)_syscall1(SYS_DUP, (int32_t)fd);
}

/* dup2 — duplicate oldfd to newfd; returns newfd, -1 on error. */
static inline int dup2(int oldfd, int newfd) {
    return (int)_syscall2(SYS_DUP2, (int32_t)oldfd, (int32_t)newfd);
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
char  *strchr(const char *s, int c);
void  *memset(void *dst, int c, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
int    memcmp(const void *a, const void *b, size_t n);

/* ── stdlib ─────────────────────────────────────────────────────────────── */
void  exit(int code);
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

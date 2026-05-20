#pragma once
#include <stdint.h>

/* Syscall numbers — eax on entry to int 0x80 */
#define SYS_EXIT   0
#define SYS_READ   1
#define SYS_WRITE  2
#define SYS_OPEN   3
#define SYS_CLOSE  4
#define SYS_LSEEK   5
#define SYS_GETPID  6
#define SYS_BRK     7
#define SYS_EXEC    8   /* spawn user process from path; returns new pid */
#define SYS_READDIR 9   /* vfs_readdir exposed to user space */
#define SYS_UNLINK  10  /* vfs_unlink exposed to user space */
#define SYS_WAITPID 11  /* block until pid is dead */
#define SYS_PIPE    12  /* create anonymous pipe; arg = int[2] for fds */
#define SYS_DUP     13  /* duplicate fd to next free slot */
#define SYS_DUP2    14  /* duplicate fd to a specific slot */

/* Install the int 0x80 IDT gate. */
void syscall_init(void);

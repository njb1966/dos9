#pragma once
#include <stdint.h>

/* Syscall numbers — eax on entry to int 0x80 */
#define SYS_EXIT   0
#define SYS_READ   1
#define SYS_WRITE  2
#define SYS_OPEN   3
#define SYS_CLOSE  4

/* Install the int 0x80 IDT gate. */
void syscall_init(void);

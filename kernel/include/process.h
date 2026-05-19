#pragma once
#include <stdint.h>

#define MAX_PROCS     8

/* process_t.state values */
#define PROC_RUNNING  0
#define PROC_READY    1
#define PROC_DEAD     2

/*
 * Per-process state.
 *
 * Field offsets are depended on by switch.S and must not change:
 *   offset 0: esp      (saved kernel stack pointer)
 *   offset 4: page_dir (physical CR3 value)
 */
typedef struct process {
    uint32_t    esp;            /* offset  0 */
    uint32_t    page_dir;       /* offset  4 */
    uint32_t    pid;            /* offset  8 */
    uint8_t     state;          /* offset 12 */
    uint8_t     _pad[3];        /* offset 13 */
    char        name[16];       /* offset 16 */
    void      (*entry)(void);   /* offset 32 */
    uint8_t    *stack;          /* offset 36 */
} process_t;

void       process_init(void);
process_t *process_create(void (*entry)(void), const char *name);
void       schedule(void);
void       process_exit(void);
int        process_count(void);
process_t *process_get(int idx);

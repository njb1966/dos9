#pragma once
#include <stdint.h>

void     shell_run(void);
uint32_t shell_exec_user(const char *path); /* returns pid, or 0 on failure */

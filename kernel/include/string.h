#pragma once
#include <stdint.h>
#include <stddef.h>

size_t  strlen(const char *s);
int     strcmp(const char *a, const char *b);
char   *strncpy(char *dst, const char *src, size_t n);
void   *memset(void *dst, int c, size_t n);
void   *memcpy(void *dst, const void *src, size_t n);

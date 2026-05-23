#pragma once

void terminal_init(void);
void terminal_putchar(char c);
void terminal_write(const char *s);
void terminal_writehex(unsigned long n);
void terminal_writedec(unsigned long n);

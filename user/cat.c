#include <dos9.h>

/*
 * cat — read from stdin (or a named file) and write to stdout.
 * With no argument: reads stdin until EOF (useful as the right side of a pipe).
 * With a path argument: reads the file (same as the shell's cat built-in).
 */
int main(void) {
    char buf[256];
    int  n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, (size_t)n);
    return 0;
}

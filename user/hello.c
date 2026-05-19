#include <dos9.h>

int main(void) {
    puts("Hello from user space! (C libc)");
    printf("  arch:    i386, ring 3\n");
    printf("  write:   SYS_WRITE = %d\n", SYS_WRITE);
    printf("  exit:    SYS_EXIT  = %d\n", SYS_EXIT);
    printf("  libc:    dos9.h, crt0.S, stdio/string/stdlib\n");
    return 0;
}

#include <dos9.h>

int main(void) {
    puts("Hello from user space!");
    printf("  pid:     %d\n", getpid());
    printf("  arch:    i386, ring 3\n");
    printf("  syscalls: exit/read/write/open/close/lseek/getpid\n");
    return 0;
}

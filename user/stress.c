#include <dos9.h>

int main(void) {
    enum { CHUNK = 1024 * 1024 };
    int blocks = 0;

    puts("stress: growing heap");
    for (;;) {
        void *p = sbrk(CHUNK);
        if (p == (void *)-1) break;
        blocks++;
        if ((blocks & 7) == 0)
            printf("stress: %d MiB\n", blocks);
        if (blocks >= 160) break;
    }

    printf("stress: brk stopped at %d MiB\n", blocks);

    int pid = exec("/disk/hello");
    if (pid < 0) {
        puts("stress: exec failed after brk pressure");
    } else {
        int rc = waitpid(pid);
        printf("stress: exec exit %d\n", rc);
    }

    puts("stress: done");
    return 0;
}

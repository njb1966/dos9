#include <dos9.h>

int main(void) {
    void *p = calloc(0x40000001u, 8u);
    if (p) {
        puts("calloc overflow bug");
        free(p);
        return 1;
    }

    puts("calloc overflow guarded");
    return 0;
}

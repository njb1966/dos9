#include <dos9.h>

int main(void) {
    puts("Hello from user space!");
    printf("  pid:  %d\n", getpid());

    /* Verify malloc/free round-trip. */
    char *buf = malloc(64);
    if (buf) {
        strcpy(buf, "malloc works");
        printf("  heap: %s\n", buf);
        free(buf);
    } else {
        puts("  heap: malloc failed");
    }

    /* calloc — zero-initialised. */
    int *arr = calloc(4, sizeof(int));
    if (arr) {
        arr[0] = 1; arr[1] = 2; arr[2] = 3; arr[3] = 4;
        printf("  arr:  %d %d %d %d\n", arr[0], arr[1], arr[2], arr[3]);
        free(arr);
    }

    return 0;
}

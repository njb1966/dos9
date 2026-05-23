#include <dos9.h>

int main(int argc, const char **argv) {
    printf("argc=%d\n", argc);
    for (int i = 0; i < argc; i++)
        printf("argv[%d]=%s\n", i, argv[i] ? argv[i] : "(null)");
    return 0;
}

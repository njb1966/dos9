#include <dos9.h>

int main(void) {
    int n1 = printf("hex=%08x\n", 1u);
    int n2 = printf("dec=%05u\n", 42u);
    int n3 = printf("neg=%6d\n", -42);
    printf("len=%d,%d,%d\n", n1, n2, n3);
    return 0;
}

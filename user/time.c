#include <dos9.h>

int main(void) {
    int fd = open("/dev/time", O_RDONLY);
    if (fd < 0) {
        puts("time: /dev/time unavailable");
        return 1;
    }

    char buf[32];
    for (;;) {
        int n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            puts("time: read failed");
            close(fd);
            return 1;
        }
        if (n == 0) break;
        write(STDOUT_FILENO, buf, (size_t)n);
    }

    write(STDOUT_FILENO, "\n", 1);

    close(fd);
    return 0;
}

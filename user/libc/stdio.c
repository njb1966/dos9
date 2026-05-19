#include <dos9.h>

void putchar(int c) {
    char ch = (char)c;
    write(STDOUT_FILENO, &ch, 1);
}

int puts(const char *s) {
    int n = (int)strlen(s);
    write(STDOUT_FILENO, s, (size_t)n);
    putchar('\n');
    return n + 1;
}

static void put_uint(uint32_t n, uint32_t base) {
    static const char digits[] = "0123456789abcdef";
    char buf[32];
    int i = 0;
    if (n == 0) { putchar('0'); return; }
    while (n) { buf[i++] = digits[n % base]; n /= base; }
    while (i--) putchar((unsigned char)buf[i]);
}

static void put_int(int32_t n) {
    if (n < 0) { putchar('-'); put_uint((uint32_t)(-n), 10); }
    else        put_uint((uint32_t)n, 10);
}

int vprintf(const char *fmt, va_list ap) {
    int count = 0;
    for (; *fmt; fmt++) {
        if (*fmt != '%') { putchar((unsigned char)*fmt); count++; continue; }
        fmt++;
        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s) { putchar((unsigned char)*s++); count++; }
            break;
        }
        case 'd': put_int(va_arg(ap, int));          count++; break;
        case 'u': put_uint(va_arg(ap, unsigned), 10); count++; break;
        case 'x': put_uint(va_arg(ap, unsigned), 16); count++; break;
        case 'c': putchar(va_arg(ap, int));            count++; break;
        case '%': putchar('%');                        count++; break;
        default:  putchar('%'); putchar((unsigned char)*fmt); count += 2; break;
        }
    }
    return count;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

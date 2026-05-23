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

static int uint_to_buf(uint32_t n, uint32_t base, char *buf) {
    static const char digits[] = "0123456789abcdef";
    int i = 0;
    if (n == 0) {
        buf[i++] = '0';
        return i;
    }
    while (n) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    return i;
}

static int put_uint(uint32_t n, uint32_t base) {
    char buf[32];
    int i = uint_to_buf(n, base, buf);
    int len = i;
    while (i--) putchar((unsigned char)buf[i]);
    return len;
}

static int put_uint_padded(uint32_t n, uint32_t base, int width, int zero_pad) {
    char buf[32];
    int len = uint_to_buf(n, base, buf);
    int digits = len;
    char pad = zero_pad ? '0' : ' ';
    while (digits < width) {
        putchar(pad);
        digits++;
    }
    while (len--) putchar((unsigned char)buf[len]);
    return digits;
}

static int put_int(int32_t n) {
    if (n < 0) {
        putchar('-');
        return 1 + put_uint((uint32_t)(-(n + 1)) + 1u, 10);
    } else {
        return put_uint((uint32_t)n, 10);
    }
}

static int put_int_padded(int32_t n, int width, int zero_pad) {
    if (n < 0) {
        char buf[32];
        uint32_t mag = (uint32_t)(-(n + 1)) + 1u;
        int len = uint_to_buf(mag, 10, buf);
        int total = len + 1;
        if (!zero_pad) {
            while (total < width) {
                putchar(' ');
                total++;
            }
            putchar('-');
        } else {
            putchar('-');
            while (total < width) {
                putchar('0');
                total++;
            }
        }

        while (len--) putchar((unsigned char)buf[len]);
        return total;
    } else {
        return put_uint_padded((uint32_t)n, 10, width, zero_pad);
    }
}

int vprintf(const char *fmt, va_list ap) {
    int count = 0;
    for (; *fmt; fmt++) {
        if (*fmt != '%') { putchar((unsigned char)*fmt); count++; continue; }
        fmt++;

        int zero_pad = 0;
        int width = 0;
        if (*fmt == '0') {
            zero_pad = 1;
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            if (width > 214748364 ||
                (width == 214748364 && (unsigned)(*fmt - '0') > 7u)) {
                width = 2147483647;
            } else {
                width = width * 10 + (*fmt - '0');
            }
            fmt++;
        }

        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            int n = 0;
            if (!s) s = "(null)";
            while (*s) { putchar((unsigned char)*s++); n++; }
            count += n;
            break;
        }
        case 'd': {
            int v = va_arg(ap, int);
            if (width > 0) count += put_int_padded(v, width, zero_pad);
            else count += put_int(v);
            break;
        }
        case 'u': {
            unsigned v = va_arg(ap, unsigned);
            if (width > 0) count += put_uint_padded(v, 10, width, zero_pad);
            else count += put_uint(v, 10);
            break;
        }
        case 'x': {
            unsigned v = va_arg(ap, unsigned);
            if (width > 0) count += put_uint_padded(v, 16, width, zero_pad);
            else count += put_uint(v, 16);
            break;
        }
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

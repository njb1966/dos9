#include <terminal.h>
#include <io.h>
#include <stdint.h>
#include <stddef.h>

/* VGA CRTC ports */
#define VGA_CRTC_ADDR 0x3D4
#define VGA_CRTC_DATA 0x3D5

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_ADDR   ((volatile uint16_t *)0xB8000)

#define FG_GREEN 10
#define BG_BLACK  0

static size_t terminal_row;
static size_t terminal_col;

static void cursor_update(void) {
    uint16_t pos = (uint16_t)(terminal_row * VGA_WIDTH + terminal_col);
    outb(VGA_CRTC_ADDR, 0x0F); outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
    outb(VGA_CRTC_ADDR, 0x0E); outb(VGA_CRTC_DATA, (uint8_t)(pos >> 8));
}

static inline uint16_t vga_entry(char c, uint8_t fg, uint8_t bg) {
    return (uint16_t)(unsigned char)c | (uint16_t)((fg | (bg << 4)) << 8);
}

void terminal_init(void) {
    terminal_row = 0;
    terminal_col = 0;
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_ADDR[y * VGA_WIDTH + x] = vga_entry(' ', FG_GREEN, BG_BLACK);

    /* enable underline cursor (scanlines 14-15) */
    outb(VGA_CRTC_ADDR, 0x0A); outb(VGA_CRTC_DATA, 0x0E);
    outb(VGA_CRTC_ADDR, 0x0B); outb(VGA_CRTC_DATA, 0x0F);
    cursor_update();
}

static void terminal_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_ADDR[(y-1) * VGA_WIDTH + x] = VGA_ADDR[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_ADDR[(VGA_HEIGHT-1) * VGA_WIDTH + x] = vga_entry(' ', FG_GREEN, BG_BLACK);
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_col = 0;
        if (++terminal_row >= VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
        cursor_update();
        return;
    }
    if (c == '\b') {
        if (terminal_col > 0) {
            terminal_col--;
            VGA_ADDR[terminal_row * VGA_WIDTH + terminal_col] =
                vga_entry(' ', FG_GREEN, BG_BLACK);
        }
        cursor_update();
        return;
    }
    VGA_ADDR[terminal_row * VGA_WIDTH + terminal_col] =
        vga_entry(c, FG_GREEN, BG_BLACK);
    if (++terminal_col >= VGA_WIDTH) {
        terminal_col = 0;
        if (++terminal_row >= VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
    }
    cursor_update();
}

void terminal_write(const char *s) {
    for (; *s; s++)
        terminal_putchar(*s);
}

void terminal_writehex(unsigned long n) {
    terminal_write("0x");
    int started = 0;
    for (int shift = 28; shift >= 0; shift -= 4) {
        int nibble = (n >> shift) & 0xF;
        if (nibble || started || shift == 0) {
            terminal_putchar("0123456789ABCDEF"[nibble]);
            started = 1;
        }
    }
}

void terminal_writedec(unsigned long n) {
    if (n == 0) { terminal_putchar('0'); return; }
    char buf[20];
    int i = 0;
    while (n) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i--) terminal_putchar(buf[i]);
}

#include <terminal.h>
#include <io.h>
#include <stdint.h>
#include <stddef.h>

#define VGA_CRTC_ADDR  0x3D4
#define VGA_CRTC_DATA  0x3D5

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_ADDR   ((volatile uint16_t *)0xC00B8000)

/* Default colour (bright green on black — DOS/9 theme) */
#define COL_FG_DEF  10   /* light green */
#define COL_BG_DEF   0   /* black       */

static size_t  term_row;
static size_t  term_col;
static uint8_t cur_fg   = COL_FG_DEF;
static uint8_t cur_bg   = COL_BG_DEF;
static int     cur_hidden;
static int     saved_row;
static int     saved_col;

/* ── ANSI CSI parser state ───────────────────────────────────────────── */
typedef enum { TS_NORM, TS_ESC, TS_CSI } ts_t;
static ts_t ts   = TS_NORM;
static uint32_t tp[8];   /* parameter values (up to 8) */
static int  tnp;     /* number of params accumulated */
static int  tdec;    /* DEC private ('?') prefix     */
static int  tdrop;   /* ignore extra params beyond tp[7] */
static int  tbold;   /* bold/bright flag              */

/* ANSI colour index → VGA colour attribute.
   ANSI: 0=Black 1=Red 2=Green 3=Yellow 4=Blue 5=Magenta 6=Cyan 7=White
   VGA:  0=Black 1=Blue 2=Green 3=Cyan  4=Red  5=Magenta 6=Brown 7=LGray */
static const uint8_t a2v[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

/* ── COM1 serial mirror (for QEMU -nographic / automated testing) ─────── */
#define COM1_BASE  0x3F8
static int com1_ready;

static int com1_probe(void) {
    uint8_t ier = inb(COM1_BASE + 1);
    uint8_t lcr = inb(COM1_BASE + 3);
    uint8_t mcr = inb(COM1_BASE + 4);

    outb(COM1_BASE + 7, 0x55);
    if (inb(COM1_BASE + 7) != 0x55) {
        outb(COM1_BASE + 1, ier);
        outb(COM1_BASE + 3, lcr);
        outb(COM1_BASE + 4, mcr);
        return 0;
    }

    outb(COM1_BASE + 1, ier);
    outb(COM1_BASE + 3, lcr);
    outb(COM1_BASE + 4, mcr);
    return 1;
}

static void com1_init(void) {
    if (!com1_probe()) return;
    outb(COM1_BASE + 1, 0x00);  /* disable interrupts */
    outb(COM1_BASE + 3, 0x80);  /* DLAB on */
    outb(COM1_BASE + 0, 0x01);  /* 115200 baud (divisor lo) */
    outb(COM1_BASE + 1, 0x00);  /* divisor hi */
    outb(COM1_BASE + 3, 0x03);  /* 8N1, DLAB off */
    outb(COM1_BASE + 2, 0xC7);  /* FIFO on */
    com1_ready = 1;
}

static void com1_putc(char c) {
    if (!com1_ready) return;
    outb(COM1_BASE, (uint8_t)c);
}

/* ── Helpers ─────────────────────────────────────────────────────────── */

static void cursor_update(void) {
    uint16_t pos = cur_hidden
        ? (uint16_t)(VGA_HEIGHT * VGA_WIDTH)   /* park off-screen */
        : (uint16_t)(term_row * VGA_WIDTH + term_col);
    outb(VGA_CRTC_ADDR, 0x0F); outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
    outb(VGA_CRTC_ADDR, 0x0E); outb(VGA_CRTC_DATA, (uint8_t)(pos >> 8));
}

static inline uint16_t vga_entry(char c, uint8_t fg, uint8_t bg) {
    return (uint16_t)(unsigned char)c | (uint16_t)((fg | (bg << 4)) << 8);
}

void terminal_init(void) {
    com1_init();
    term_row = 0; term_col = 0;
    cur_fg = COL_FG_DEF; cur_bg = COL_BG_DEF;
    cur_hidden = 0; saved_row = 0; saved_col = 0;
    tbold = 0; ts = TS_NORM; tnp = 0; tdec = 0;
    tdrop = 0;

    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_ADDR[y * VGA_WIDTH + x] = vga_entry(' ', cur_fg, cur_bg);

    /* underline cursor (scanlines 14-15) */
    outb(VGA_CRTC_ADDR, 0x0A); outb(VGA_CRTC_DATA, 0x0E);
    outb(VGA_CRTC_ADDR, 0x0B); outb(VGA_CRTC_DATA, 0x0F);
    cursor_update();
}

static void term_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_ADDR[(y-1)*VGA_WIDTH+x] = VGA_ADDR[y*VGA_WIDTH+x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_ADDR[(VGA_HEIGHT-1)*VGA_WIDTH+x] = vga_entry(' ', cur_fg, cur_bg);
}

/* ── CSI dispatch ────────────────────────────────────────────────────── */

static void sgr_apply(void) {
    if (tnp == 0) {
        cur_fg = COL_FG_DEF; cur_bg = COL_BG_DEF; tbold = 0;
        return;
    }
    for (int i = 0; i < tnp; i++) {
        int v = tp[i];
        if (v == 0) {
            cur_fg = COL_FG_DEF; cur_bg = COL_BG_DEF; tbold = 0;
        } else if (v == 1) {
            tbold = 1; cur_fg = (uint8_t)((cur_fg & 7) | 8);
        } else if (v == 22) {
            tbold = 0; cur_fg = (uint8_t)(cur_fg & 7);
        } else if (v >= 30 && v <= 37) {
            cur_fg = (uint8_t)(a2v[v-30] | (uint8_t)(tbold ? 8 : 0));
        } else if (v >= 40 && v <= 47) {
            cur_bg = a2v[v-40];
        } else if (v >= 90 && v <= 97) {
            cur_fg = (uint8_t)(a2v[v-90] | 8); tbold = 1;
        } else if (v >= 100 && v <= 107) {
            cur_bg = (uint8_t)(a2v[v-100] | 8);
        }
    }
}

static void csi_dispatch(char final) {
    /* For cursor movement, absent/zero param defaults to 1. */
    int p0 = (tnp >= 1 && tp[0]) ? tp[0] : 1;
    int p1 = (tnp >= 2 && tp[1]) ? tp[1] : 1;

    if (tdec) {
        if (tp[0] == 25) {
            cur_hidden = (final == 'l') ? 1 : 0;
            cursor_update();
        }
        return;
    }

    switch (final) {
    case 'H': case 'f': {   /* CUP — cursor position (1-indexed) */
        size_t r = (size_t)(p0 - 1);
        size_t c = (size_t)(p1 - 1);
        if (r >= VGA_HEIGHT) r = VGA_HEIGHT - 1;
        if (c >= VGA_WIDTH)  c = VGA_WIDTH  - 1;
        term_row = r; term_col = c;
        cursor_update();
        break;
    }
    case 'A':   /* CUU — cursor up */
        if (term_row >= (size_t)p0) term_row -= (size_t)p0; else term_row = 0;
        cursor_update(); break;
    case 'B':   /* CUD — cursor down */
        term_row += (size_t)p0;
        if (term_row >= VGA_HEIGHT) term_row = VGA_HEIGHT - 1;
        cursor_update(); break;
    case 'C':   /* CUF — cursor forward */
        term_col += (size_t)p0;
        if (term_col >= VGA_WIDTH) term_col = VGA_WIDTH - 1;
        cursor_update(); break;
    case 'D':   /* CUB — cursor back */
        if (term_col >= (size_t)p0) term_col -= (size_t)p0; else term_col = 0;
        cursor_update(); break;
    case 'J': {  /* ED — erase display */
        int mode = tnp >= 1 ? tp[0] : 0;
        if (mode == 2) {
            for (size_t y = 0; y < VGA_HEIGHT; y++)
                for (size_t x = 0; x < VGA_WIDTH; x++)
                    VGA_ADDR[y*VGA_WIDTH+x] = vga_entry(' ', cur_fg, cur_bg);
            term_row = 0; term_col = 0; cursor_update();
        } else if (mode == 0) {
            for (size_t x = term_col; x < VGA_WIDTH; x++)
                VGA_ADDR[term_row*VGA_WIDTH+x] = vga_entry(' ', cur_fg, cur_bg);
            for (size_t y = term_row+1; y < VGA_HEIGHT; y++)
                for (size_t x = 0; x < VGA_WIDTH; x++)
                    VGA_ADDR[y*VGA_WIDTH+x] = vga_entry(' ', cur_fg, cur_bg);
        }
        break;
    }
    case 'K': {  /* EL — erase line */
        int mode = tnp >= 1 ? tp[0] : 0;
        if (mode == 0) {
            for (size_t x = term_col; x < VGA_WIDTH; x++)
                VGA_ADDR[term_row*VGA_WIDTH+x] = vga_entry(' ', cur_fg, cur_bg);
        } else if (mode == 1) {
            for (size_t x = 0; x <= term_col; x++)
                VGA_ADDR[term_row*VGA_WIDTH+x] = vga_entry(' ', cur_fg, cur_bg);
        } else if (mode == 2) {
            for (size_t x = 0; x < VGA_WIDTH; x++)
                VGA_ADDR[term_row*VGA_WIDTH+x] = vga_entry(' ', cur_fg, cur_bg);
        }
        break;
    }
    case 'm':
        sgr_apply();
        break;
    case 's':   /* DECSC — save cursor */
        saved_row = (int)term_row; saved_col = (int)term_col;
        break;
    case 'u':   /* DECRC — restore cursor */
        term_row = (size_t)saved_row; term_col = (size_t)saved_col;
        cursor_update();
        break;
    default:
        break;
    }
}

/* ── Main putchar ─────────────────────────────────────────────────────── */

void terminal_putchar(char c) {
    /* Mirror to COM1 for serial capture (QEMU -nographic / automated tests) */
    if (c == '\n') com1_putc('\r');
    com1_putc(c);

    if (ts == TS_ESC) {
        if (c == '[') {
            ts = TS_CSI;
            tnp = 0; tdec = 0;
            tdrop = 0;
            for (int i = 0; i < 8; i++) tp[i] = 0;
        } else {
            ts = TS_NORM;
            if (c == '\x1b') ts = TS_ESC;
        }
        return;
    }

    if (ts == TS_CSI) {
        if (tdrop) {
            if (c == ';' || c == '?' || (c >= '0' && c <= '9')) return;
            csi_dispatch(c);
            ts = TS_NORM;
            tdrop = 0;
            return;
        }
        if (c == '?') { tdec = 1; return; }
        if (c >= '0' && c <= '9') {
            if (tnp == 0) tnp = 1;
            if (tp[tnp-1] > 214748364u ||
                (tp[tnp-1] == 214748364u && (uint32_t)(c - '0') > 7u)) {
                tp[tnp-1] = 2147483647u;
            } else {
                tp[tnp-1] = tp[tnp-1] * 10u + (uint32_t)(c - '0');
            }
            return;
        }
        if (c == ';') {
            if (tnp == 0) tnp = 1;
            if (tnp < 8) tnp++;
            else tdrop = 1;
            return;
        }
        csi_dispatch(c);
        ts = TS_NORM;
        tdrop = 0;
        return;
    }

    /* TS_NORM */
    if (c == '\x1b') { ts = TS_ESC; return; }

    if (c == '\n') {
        term_col = 0;
        if (++term_row >= VGA_HEIGHT) { term_scroll(); term_row = VGA_HEIGHT - 1; }
        cursor_update();
        return;
    }
    if (c == '\r') {
        term_col = 0; cursor_update(); return;
    }
    if (c == '\b') {
        if (term_col > 0) {
            term_col--;
            VGA_ADDR[term_row*VGA_WIDTH+term_col] = vga_entry(' ', cur_fg, cur_bg);
        }
        cursor_update();
        return;
    }

    VGA_ADDR[term_row*VGA_WIDTH+term_col] = vga_entry(c, cur_fg, cur_bg);
    if (++term_col >= VGA_WIDTH) {
        term_col = 0;
        if (++term_row >= VGA_HEIGHT) { term_scroll(); term_row = VGA_HEIGHT - 1; }
    }
    cursor_update();
}

void terminal_write(const char *s) {
    for (; *s; s++) terminal_putchar(*s);
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
    while (n) { buf[i++] = '0' + (char)(n % 10); n /= 10; }
    while (i--) terminal_putchar(buf[i]);
}

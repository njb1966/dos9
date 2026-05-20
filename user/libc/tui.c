#include <tui.h>
#include <dos9.h>

/* VGA colour → ANSI colour index.
   VGA: 0=Black 1=Blue 2=Green 3=Cyan  4=Red  5=Magenta 6=Brown 7=LGray
   ANSI:0=Black 1=Red  2=Green 3=Yellow4=Blue 5=Magenta 6=Cyan  7=White */
static const int v2a[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

static void tui_write(const char *buf, int n) {
    write(STDOUT_FILENO, buf, (size_t)n);
}

/* Append decimal n (1-99) into buf at *pos, advance *pos. */
static void put_dec(char *buf, int *pos, int n) {
    if (n >= 10) buf[(*pos)++] = (char)('0' + n / 10);
    buf[(*pos)++] = (char)('0' + n % 10);
}

/* ── Cursor ──────────────────────────────────────────────────────────── */

void tui_cursor_hide(void) { tui_write("\x1b[?25l", 6); }
void tui_cursor_show(void) { tui_write("\x1b[?25h", 6); }

/* ── Screen ──────────────────────────────────────────────────────────── */

void tui_clear(void)   { tui_write("\x1b[2J", 4); }
void tui_clreol(void)  { tui_write("\x1b[K",  3); }
void tui_clrline(void) { tui_write("\x1b[2K", 4); }

void tui_move(int row, int col) {
    char buf[16];
    int n = 0;
    buf[n++] = '\x1b'; buf[n++] = '[';
    put_dec(buf, &n, row + 1);
    buf[n++] = ';';
    put_dec(buf, &n, col + 1);
    buf[n++] = 'H';
    tui_write(buf, n);
}

/* ── Colour ──────────────────────────────────────────────────────────── */

void tui_color(int fg, int bg) {
    int bright = (fg >= 8);
    int af = v2a[fg & 7] + (bright ? 90 : 30);   /* 30-37 or 90-97  */
    int ab = v2a[bg & 7] + 40;                    /* 40-47            */

    char buf[16];
    int n = 0;
    buf[n++] = '\x1b'; buf[n++] = '[';
    buf[n++] = '0'; buf[n++] = ';';               /* reset first       */
    buf[n++] = (char)('0' + af / 10);
    buf[n++] = (char)('0' + af % 10);
    buf[n++] = ';';
    buf[n++] = (char)('0' + ab / 10);
    buf[n++] = (char)('0' + ab % 10);
    buf[n++] = 'm';
    tui_write(buf, n);
}

void tui_reset(void) { tui_write("\x1b[0m", 4); }

/* ── Positioned output ───────────────────────────────────────────────── */

void tui_putch(int row, int col, char c, int fg, int bg) {
    tui_move(row, col);
    tui_color(fg, bg);
    tui_write(&c, 1);
}

void tui_puts(int row, int col, const char *s, int fg, int bg) {
    tui_move(row, col);
    tui_color(fg, bg);
    int len = 0;
    while (s[len]) len++;
    tui_write(s, len);
}

void tui_fill(int row, int col, int h, int w, char c, int fg, int bg) {
    char line[TUI_COLS];
    if (w > TUI_COLS) w = TUI_COLS;
    for (int x = 0; x < w; x++) line[x] = c;

    tui_color(fg, bg);
    for (int y = 0; y < h; y++) {
        tui_move(row + y, col);
        tui_write(line, w);
    }
}

/* ── Box drawing (double-line CP437) ─────────────────────────────────── */

void tui_box(int row, int col, int h, int w, int fg, int bg) {
    if (h < 2 || w < 2) return;

    char line[TUI_COLS + 2];
    int i, inner = w - 2;

    tui_color(fg, bg);

    /* top: ╔═…═╗ */
    tui_move(row, col);
    line[0] = '\xC9';
    for (i = 0; i < inner; i++) line[1 + i] = '\xCD';
    line[1 + inner] = '\xBB';
    tui_write(line, w);

    /* sides: ║ … ║ */
    for (int y = 1; y < h - 1; y++) {
        tui_move(row + y, col);         tui_write("\xBA", 1);
        tui_move(row + y, col + w - 1); tui_write("\xBA", 1);
    }

    /* bottom: ╚═…═╝ */
    tui_move(row + h - 1, col);
    line[0] = '\xC8';
    for (i = 0; i < inner; i++) line[1 + i] = '\xCD';
    line[1 + inner] = '\xBC';
    tui_write(line, w);
}

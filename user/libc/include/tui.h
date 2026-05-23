#pragma once

/* Screen dimensions */
#define TUI_ROWS 25
#define TUI_COLS 80

/* VGA 16-colour palette indices */
#define TUI_BLACK     0
#define TUI_BLUE      1
#define TUI_GREEN     2
#define TUI_CYAN      3
#define TUI_RED       4
#define TUI_MAGENTA   5
#define TUI_BROWN     6
#define TUI_LGRAY     7
#define TUI_DGRAY     8
#define TUI_LBLUE     9
#define TUI_LGREEN   10
#define TUI_LCYAN    11
#define TUI_LRED     12
#define TUI_LMAGENTA 13
#define TUI_YELLOW   14
#define TUI_WHITE    15

/* Cursor */
void tui_cursor_hide(void);
void tui_cursor_show(void);

/* Screen */
void tui_clear(void);               /* ESC[2J — clear screen, cursor to 0,0 */
void tui_move(int row, int col);    /* ESC[r;cH — 0-indexed row/col */
void tui_clreol(void);              /* ESC[K  — erase cursor to end of line */
void tui_clrline(void);             /* ESC[2K — erase entire current line    */

/* Colour (VGA 16-colour indices; bg > 7 maps to 0-7 — VGA bg limit) */
void tui_color(int fg, int bg);
void tui_reset(void);               /* ESC[0m — restore default green-on-black */

/* Positioned output */
void tui_putch(int row, int col, char c,          int fg, int bg);
void tui_puts (int row, int col, const char *s,   int fg, int bg);
void tui_fill (int row, int col, int h, int w, char c, int fg, int bg);

/* Box drawing (double-line CP437 border) */
void tui_box(int row, int col, int h, int w, int fg, int bg);

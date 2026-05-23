#include <dos9.h>
#include <tui.h>

static const char *COLOR_NAMES[16] = {
    "BLK", "BLU", "GRN", "CYN", "RED", "MAG", "BRN", "LGY",
    "DGY", "LBL", "LGN", "LCY", "LRD", "LMG", "YLW", "WHT"
};

int main(void) {
    tui_cursor_hide();

    /* Blue background */
    tui_fill(0, 0, TUI_ROWS, TUI_COLS, ' ', TUI_LGRAY, TUI_BLUE);

    /* Outer border */
    tui_box(0, 0, TUI_ROWS, TUI_COLS, TUI_WHITE, TUI_BLUE);

    /* Title bar */
    tui_fill(1, 1, 1, TUI_COLS - 2, ' ', TUI_BLACK, TUI_CYAN);
    tui_puts(1, 2, " DOS/9  TUI Toolkit Demo ", TUI_BLACK, TUI_CYAN);

    /* Colour palette section */
    tui_puts(3, 3, "Foreground colours:", TUI_WHITE, TUI_BLUE);
    for (int i = 0; i < 16; i++) {
        int col = 3 + (i % 8) * 9;
        int row = 4 + (i / 8);
        tui_putch(row, col,     ' ',              i,        TUI_BLUE);
        tui_puts (row, col + 1, COLOR_NAMES[i],   i,        TUI_BLUE);
        tui_putch(row, col + 4, ' ',              i,        TUI_BLUE);
    }

    /* Background colours */
    tui_puts(7, 3, "Background colours:", TUI_WHITE, TUI_BLUE);
    for (int i = 0; i < 8; i++) {
        int col = 3 + i * 9;
        tui_putch(8, col,     ' ',            TUI_WHITE, i);
        tui_puts (8, col + 1, COLOR_NAMES[i], TUI_WHITE, i);
        tui_putch(8, col + 4, ' ',            TUI_WHITE, i);
    }

    /* Box-drawing demo */
    tui_puts(10, 3, "Box drawing:", TUI_WHITE, TUI_BLUE);
    tui_box(11, 3,  5, 18, TUI_YELLOW,  TUI_BLUE);
    tui_puts(13, 5, "double-line", TUI_LGRAY, TUI_BLUE);
    tui_box(11, 23, 5, 18, TUI_LCYAN,   TUI_BLUE);
    tui_puts(13, 25, "inner fill", TUI_LGRAY, TUI_BLUE);
    tui_fill(12, 24, 3, 16, '\xB2', TUI_BLUE, TUI_BLUE);

    tui_box(11, 43, 5, 18, TUI_LGREEN,  TUI_BLUE);
    tui_puts(13, 45, "nested box", TUI_LGRAY, TUI_BLUE);
    tui_box(12, 45, 3, 14, TUI_BROWN,   TUI_BLUE);

    tui_box(11, 63, 5, 14, TUI_LRED,    TUI_BLUE);
    tui_puts(13, 65, "status", TUI_LGRAY, TUI_BLUE);

    /* Text rendering test */
    tui_puts(17, 3, "Text rendering:", TUI_WHITE, TUI_BLUE);
    tui_puts(18, 3, "Normal", TUI_LGRAY, TUI_BLUE);
    tui_puts(18, 12, "Bold/Bright", TUI_WHITE, TUI_BLUE);
    tui_puts(18, 26, "Warning", TUI_YELLOW, TUI_BLUE);
    tui_puts(18, 36, "Error", TUI_LRED, TUI_BLUE);
    tui_puts(18, 44, "OK", TUI_LGREEN, TUI_BLUE);
    tui_puts(18, 49, "Info", TUI_LCYAN, TUI_BLUE);
    tui_puts(18, 56, "Highlight", TUI_BLACK, TUI_CYAN);

    /* Status bar */
    tui_fill(23, 1, 1, TUI_COLS - 2, ' ', TUI_BLACK, TUI_LGRAY);
    tui_puts(23, 2,  "F1=Help", TUI_BLACK, TUI_LGRAY);
    tui_puts(23, 12, "F10=Exit", TUI_BLACK, TUI_LGRAY);
    tui_puts(23, 33, "Press any key to exit", TUI_DGRAY, TUI_LGRAY);

    /* Park cursor in status bar */
    tui_move(23, 56);
    tui_cursor_show();

    /* Wait for keypress */
    char ch;
    read(STDIN_FILENO, &ch, 1);

    /* Restore default terminal state */
    tui_reset();
    tui_clear();

    return 0;
}

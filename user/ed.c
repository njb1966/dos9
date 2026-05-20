/*
 * ed.c — DOS/9 full-screen text editor.
 *
 * Layout:
 *   Row  0: title bar  (WHITE on BLUE)
 *   Rows 1-22: editing area
 *   Row 23: status bar (BLACK on LGRAY)
 *   Row 24: help bar   (BLACK on LGRAY)
 */

#include <dos9.h>
#include <tui.h>

/* ── Constants ────────────────────────────────────────────────────────── */

#define MAX_LINES  300
#define LINE_CAP   128
#define EDIT_ROWS   22
#define TITLE_ROW    0
#define EDIT_START   1
#define STATUS_ROW  23
#define HELP_ROW    24

/* ── Buffer ───────────────────────────────────────────────────────────── */

static char buf[MAX_LINES][LINE_CAP];
static int  llen[MAX_LINES];
static int  nlines;
static int  cx, cy, top;
static int  modified;
static char filename[128];
static char status_msg[64];
/* savebuf: MAX_LINES * (LINE_CAP + 1) = 300 * 129 = 38700 bytes */
static char savebuf[MAX_LINES * (LINE_CAP + 1)];

/* ── Key codes ────────────────────────────────────────────────────────── */

#define KEY_UP    0x100
#define KEY_DOWN  0x101
#define KEY_LEFT  0x102
#define KEY_RIGHT 0x103

/* ── Small helpers ────────────────────────────────────────────────────── */

/* Simple integer → string, writes into dst, returns number of chars written. */
static int itoa_dec(int n, char *dst) {
    if (n < 0) { dst[0] = '-'; return 1 + itoa_dec(-n, dst + 1); }
    char tmp[12];
    int len = 0;
    if (n == 0) { dst[0] = '0'; return 1; }
    while (n > 0) { tmp[len++] = (char)('0' + n % 10); n /= 10; }
    for (int i = 0; i < len; i++) dst[i] = tmp[len - 1 - i];
    return len;
}

/* ── Input ────────────────────────────────────────────────────────────── */

static int read_key(void) {
    char ch;
    if (read(STDIN_FILENO, &ch, 1) != 1) return 0;
    if ((unsigned char)ch != 0x1b) return (unsigned char)ch;

    /* Try to read escape sequence. */
    char ch2;
    if (read(STDIN_FILENO, &ch2, 1) != 1) return 0x1b;
    if (ch2 != '[') return (unsigned char)ch2;
    char ch3;
    if (read(STDIN_FILENO, &ch3, 1) != 1) return 0;
    switch (ch3) {
    case 'A': return KEY_UP;
    case 'B': return KEY_DOWN;
    case 'C': return KEY_RIGHT;
    case 'D': return KEY_LEFT;
    }
    return 0;
}

/* ── File I/O ─────────────────────────────────────────────────────────── */

static void load_file(void) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        /* New file — start with one empty line. */
        nlines = 1;
        buf[0][0] = '\0';
        llen[0] = 0;
        return;
    }

    /* Read file into savebuf. */
    int total = 0;
    int n;
    int maxsz = (int)sizeof(savebuf) - 1;
    while (total < maxsz &&
           (n = read(fd, savebuf + total, (size_t)(maxsz - total))) > 0) {
        total += n;
    }
    close(fd);
    savebuf[total] = '\0';

    /* Split into lines. */
    nlines = 0;
    char *p = savebuf;
    char *end = savebuf + total;
    while (p <= end && nlines < MAX_LINES - 1) {
        char *nl = p;
        while (nl < end && *nl != '\n') nl++;
        int len = (int)(nl - p);
        if (len >= LINE_CAP) len = LINE_CAP - 1;
        memcpy(buf[nlines], p, (size_t)len);
        buf[nlines][len] = '\0';
        llen[nlines] = len;
        nlines++;
        p = nl + 1;
        if (nl >= end) break;
    }
    if (nlines == 0) {
        nlines = 1;
        buf[0][0] = '\0';
        llen[0] = 0;
    }
}

static int save_file(void) {
    /* Build content in savebuf. */
    int pos = 0;
    int maxsz = (int)sizeof(savebuf);
    for (int i = 0; i < nlines; i++) {
        if (i > 0) {
            if (pos >= maxsz - 1) break;
            savebuf[pos++] = '\n';
        }
        int l = llen[i];
        if (pos + l >= maxsz) l = maxsz - pos - 1;
        memcpy(savebuf + pos, buf[i], (size_t)l);
        pos += l;
    }
    int total = pos;

    int fd = open(filename, O_WRONLY | O_CREAT);
    if (fd < 0) return -1;

    int written = 0;
    while (written < total) {
        int n = write(fd, savebuf + written, (size_t)(total - written));
        if (n <= 0) { close(fd); return -1; }
        written += n;
    }
    close(fd);
    modified = 0;
    return 0;
}

/* ── Editing operations ───────────────────────────────────────────────── */

static void clamp_cx(void) {
    if (cx > llen[cy]) cx = llen[cy];
}

static void scroll(void) {
    if (cy < top) top = cy;
    if (cy >= top + EDIT_ROWS) top = cy - EDIT_ROWS + 1;
}

static void insert_char(char c) {
    if (llen[cy] >= LINE_CAP - 1) return;
    /* Shift right from cx. */
    for (int i = llen[cy]; i > cx; i--)
        buf[cy][i] = buf[cy][i - 1];
    buf[cy][cx] = c;
    llen[cy]++;
    buf[cy][llen[cy]] = '\0';
    cx++;
    modified = 1;
}

static void delete_back(void) {
    if (cx > 0) {
        /* Shift left. */
        for (int i = cx - 1; i < llen[cy]; i++)
            buf[cy][i] = buf[cy][i + 1];
        llen[cy]--;
        buf[cy][llen[cy]] = '\0';
        cx--;
        modified = 1;
    } else if (cy > 0) {
        /* Join with previous line. */
        int prev_len = llen[cy - 1];
        int cur_len  = llen[cy];
        if (prev_len + cur_len < LINE_CAP - 1) {
            memcpy(buf[cy - 1] + prev_len, buf[cy], (size_t)cur_len);
            llen[cy - 1] = prev_len + cur_len;
            buf[cy - 1][llen[cy - 1]] = '\0';
            /* Remove line cy. */
            for (int i = cy; i < nlines - 1; i++) {
                memcpy(buf[i], buf[i + 1], (size_t)llen[i + 1] + 1);
                llen[i] = llen[i + 1];
            }
            nlines--;
            cy--;
            cx = prev_len;
            modified = 1;
        }
    }
}

static void insert_newline(void) {
    if (nlines >= MAX_LINES - 1) return;
    /* Shift lines down. */
    for (int i = nlines; i > cy + 1; i--) {
        memcpy(buf[i], buf[i - 1], (size_t)llen[i - 1] + 1);
        llen[i] = llen[i - 1];
    }
    /* Split current line at cx. */
    int after = llen[cy] - cx;
    memcpy(buf[cy + 1], buf[cy] + cx, (size_t)after);
    buf[cy + 1][after] = '\0';
    llen[cy + 1] = after;
    buf[cy][cx] = '\0';
    llen[cy] = cx;
    nlines++;
    cy++;
    cx = 0;
    modified = 1;
}

static void kill_line(void) {
    if (cx < llen[cy]) {
        /* Truncate at cx. */
        buf[cy][cx] = '\0';
        llen[cy] = cx;
        modified = 1;
    } else if (cy < nlines - 1) {
        /* Join next line. */
        int cur_len  = llen[cy];
        int next_len = llen[cy + 1];
        if (cur_len + next_len < LINE_CAP - 1) {
            memcpy(buf[cy] + cur_len, buf[cy + 1], (size_t)next_len);
            llen[cy] = cur_len + next_len;
            buf[cy][llen[cy]] = '\0';
            for (int i = cy + 1; i < nlines - 1; i++) {
                memcpy(buf[i], buf[i + 1], (size_t)llen[i + 1] + 1);
                llen[i] = llen[i + 1];
            }
            nlines--;
            modified = 1;
        }
    }
}

/* ── Rendering ────────────────────────────────────────────────────────── */

static void render_title(void) {
    tui_fill(TITLE_ROW, 0, 1, TUI_COLS, ' ', TUI_WHITE, TUI_BLUE);
    tui_puts(TITLE_ROW, 1, " ed: ", TUI_WHITE, TUI_BLUE);
    int flen = (int)strlen(filename);
    tui_puts(TITLE_ROW, 6, filename, TUI_WHITE, TUI_BLUE);
    if (modified) {
        tui_puts(TITLE_ROW, 6 + flen + 1, "[*]", TUI_YELLOW, TUI_BLUE);
    }
}

static void render_line(int screen_row, int buf_row) {
    tui_move(screen_row, 0);
    tui_color(TUI_LGRAY, TUI_BLACK);
    if (buf_row >= 0 && buf_row < nlines) {
        char line[80];
        int len = llen[buf_row];
        if (len > 79) len = 79;
        memcpy(line, buf[buf_row], (size_t)len);
        write(STDOUT_FILENO, line, (size_t)len);
        tui_clreol();
    } else {
        tui_clrline();
    }
}

static void render_status(void) {
    tui_fill(STATUS_ROW, 0, 1, TUI_COLS, ' ', TUI_BLACK, TUI_LGRAY);
    tui_move(STATUS_ROW, 1);
    tui_color(TUI_BLACK, TUI_LGRAY);

    /* Build "L:NNN/NNN  C:NNN" */
    char tmp[64];
    int pos = 0;
    tmp[pos++] = 'L';
    tmp[pos++] = ':';
    pos += itoa_dec(cy + 1, tmp + pos);
    tmp[pos++] = '/';
    pos += itoa_dec(nlines, tmp + pos);
    tmp[pos++] = ' ';
    tmp[pos++] = ' ';
    tmp[pos++] = 'C';
    tmp[pos++] = ':';
    pos += itoa_dec(cx + 1, tmp + pos);

    write(STDOUT_FILENO, tmp, (size_t)pos);

    if (status_msg[0]) {
        write(STDOUT_FILENO, " | ", 3);
        write(STDOUT_FILENO, status_msg, strlen(status_msg));
    }
}

static void render_help(void) {
    tui_fill(HELP_ROW, 0, 1, TUI_COLS, ' ', TUI_BLACK, TUI_LGRAY);
    tui_puts(HELP_ROW, 1,
             "^S=Save  ^Q=Quit  ^K=CutLine  Arrows=Move  Enter=Newline  BS=Delete",
             TUI_BLACK, TUI_LGRAY);
}

static void render_all(void) {
    render_title();
    for (int sr = 1; sr <= EDIT_ROWS; sr++)
        render_line(sr, top + sr - 1);
    render_status();
    render_help();
    tui_move(cy - top + EDIT_START, cx);
    tui_cursor_show();
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc < 2) {
        puts("usage: ed <path>");
        return 1;
    }

    strncpy(filename, argv[1], 127);
    filename[127] = '\0';

    nlines   = 1;
    buf[0][0] = '\0';
    llen[0]  = 0;
    cx = cy = top = modified = 0;
    status_msg[0] = '\0';

    load_file();

    tui_cursor_hide();
    render_all();
    tui_cursor_show();

    int quit_confirm = 0;

    for (;;) {
        int k = read_key();

        /* Handle quit_confirm: any key other than Ctrl+Q clears it. */
        if (k != 17 && quit_confirm) {
            quit_confirm = 0;
            status_msg[0] = '\0';
        }

        switch (k) {

        case KEY_UP:
            if (cy > 0) { cy--; clamp_cx(); scroll(); }
            break;

        case KEY_DOWN:
            if (cy < nlines - 1) { cy++; clamp_cx(); scroll(); }
            break;

        case KEY_LEFT:
            if (cx > 0) cx--;
            else if (cy > 0) { cy--; cx = llen[cy]; scroll(); }
            break;

        case KEY_RIGHT:
            if (cx < llen[cy]) cx++;
            else if (cy < nlines - 1) { cy++; cx = 0; scroll(); }
            break;

        case '\r': case '\n':
            insert_newline();
            scroll();
            break;

        case '\b': case 127:
            delete_back();
            scroll();
            break;

        case 19:  /* Ctrl+S — save */
            if (save_file() == 0) {
                strncpy(status_msg, "Saved.", 63);
                status_msg[63] = '\0';
            } else {
                strncpy(status_msg, "Save FAILED.", 63);
                status_msg[63] = '\0';
            }
            break;

        case 17:  /* Ctrl+Q — quit */
            if (modified && !quit_confirm) {
                quit_confirm = 1;
                strncpy(status_msg, "Unsaved! Ctrl+Q again to quit.", 63);
                status_msg[63] = '\0';
            } else {
                goto done;
            }
            break;

        case 11:  /* Ctrl+K — cut line */
            kill_line();
            break;

        default:
            if (k >= 32 && k < 256) {
                insert_char((char)k);
            }
            break;
        }

        render_all();
    }

done:
    tui_cursor_show();
    tui_reset();
    tui_clear();
    return 0;
}

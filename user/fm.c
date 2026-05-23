/*
 * fm.c — DOS/9 file manager
 * Norton Commander-style two-pane navigator.
 *
 * Controls: Up/Down=navigate  Tab/Left/Right=switch pane
 *           Enter=open dir or exec file  Backspace=parent dir
 *           R=reload  Q=quit
 */

#include <dos9.h>
#include <tui.h>

/* ── Layout ──────────────────────────────────────────────────────────── */
#define LEFT_COL     0
#define RIGHT_COL   40
#define PANE_W      40
#define HDR_ROW      1    /* path display row inside each pane        */
#define SEP_ROW      2    /* ╠═══╣ separator                          */
#define CONTENT_ROW  3    /* first file-listing row                   */
#define CONTENT_H   19    /* visible entry rows (rows 3-21)           */
#define BOTTOM_ROW  22    /* bottom border of pane boxes              */
#define INFO_ROW    23    /* selected-file info bar                   */
#define FKEY_ROW    24    /* key-binding reminder bar                 */

/* ── Colours ─────────────────────────────────────────────────────────── */
#define C_BORD_ACT   TUI_WHITE    /* active pane border fg   */
#define C_BORD_IDLE  TUI_LGRAY   /* inactive pane border fg  */
#define C_PANE_BG    TUI_BLUE
#define C_FILE_FG    TUI_LGRAY
#define C_DIR_FG     TUI_WHITE
#define C_SEL_FG     TUI_BLACK
#define C_SEL_BG     TUI_CYAN
#define C_ISEL_FG    TUI_BLACK
#define C_ISEL_BG    TUI_LGRAY

/* ── Data model ──────────────────────────────────────────────────────── */
#define MAX_ENTRIES 64
#define NAME_SZ     16
#define PATH_SZ    128

typedef struct {
    char name[NAME_SZ];
    int  is_dir;
    int  size;           /* bytes; -1 = unknown */
} Entry;

typedef struct {
    char  path[PATH_SZ];
    Entry ents[MAX_ENTRIES];
    int   count;
    int   cursor;
    int   top;
} Panel;

static Panel panels[2];
static int   active;     /* 0 = left, 1 = right */

/* ── Key codes ───────────────────────────────────────────────────────── */
#define KEY_UP    0x100
#define KEY_DOWN  0x101
#define KEY_LEFT  0x102
#define KEY_RIGHT 0x103

/* ── Path helpers ────────────────────────────────────────────────────── */

/* Append "/" + name to base. Returns 0 on success, -1 if the result would
   not fit in `out`. */
static int path_join(char *out, const char *base, const char *name, int maxn) {
    char tmp[PATH_SZ];
    int i = 0;
    if (maxn > PATH_SZ) return -1;
    if (base[0] == '/' && base[1] == '\0') {
        tmp[i++] = '/';
    } else {
        for (; base[i]; i++) {
            if (i >= maxn - 2) return -1;
            tmp[i] = base[i];
        }
        if (i >= maxn - 2) return -1;
        tmp[i++] = '/';
    }
    for (int j = 0; name[j]; j++) {
        if (i >= maxn - 1) return -1;
        tmp[i++] = name[j];
    }
    tmp[i] = '\0';
    memcpy(out, tmp, (size_t)(i + 1));
    return 0;
}

/* Truncate path to its parent in-place. */
static void path_parent(char *path) {
    if (path[0] == '/' && path[1] == '\0') return;
    int last = 0;
    for (int i = 0; path[i]; i++)
        if (path[i] == '/') last = i;
    if (last == 0) path[1] = '\0';
    else           path[last] = '\0';
}

/* ── Directory loading ───────────────────────────────────────────────── */

/* Open path and determine whether it is a directory (readdir returns >= 0)
   or a regular file.  Also lseeks to SEEK_END to get size for files. */
static int probe_entry(const char *path, int *out_size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { *out_size = -1; return 0; }

    char buf[NAME_SZ];
    int r = readdir(fd, 0, buf, NAME_SZ);
    if (r >= 0) {          /* readdir succeeded → directory */
        close(fd);
        *out_size = -1;
        return 1;
    }
    /* Not a directory — try to get file size */
    int sz = lseek(fd, 0, SEEK_END);
    close(fd);
    *out_size = (sz >= 0) ? sz : -1;
    return 0;
}

static void load_panel(Panel *p) {
    p->count = 0;

    /* Add ".." for non-root directories */
    if (!(p->path[0] == '/' && p->path[1] == '\0')) {
        strcpy(p->ents[0].name, "..");
        p->ents[0].is_dir = 1;
        p->ents[0].size   = -1;
        p->count = 1;
    }

    int fd = open(p->path, O_RDONLY);
    if (fd < 0) return;

    for (int idx = 0; p->count < MAX_ENTRIES; idx++) {
        char buf[PATH_SZ];
        if (readdir(fd, (uint32_t)idx, buf, sizeof(buf)) <= 0) break;
        if ((int)strlen(buf) >= NAME_SZ) continue;
        memcpy(p->ents[p->count].name, buf, strlen(buf) + 1u);

        char full[PATH_SZ];
        if (path_join(full, p->path, buf, PATH_SZ) < 0) {
            p->ents[p->count].is_dir = 0;
            p->ents[p->count].size   = -1;
            p->count++;
            continue;
        }
        int sz;
        p->ents[p->count].is_dir = probe_entry(full, &sz);
        p->ents[p->count].size   = sz;
        p->count++;
    }
    close(fd);

    if (p->cursor >= p->count) p->cursor = p->count > 0 ? p->count - 1 : 0;
    if (p->top    >  p->cursor) p->top   = p->cursor;
}

/* ── Drawing ─────────────────────────────────────────────────────────── */

/* Write 7-char right-justified size into buf7 (no null terminator). */
static void fmt_size(char *buf7, int sz) {
    if (sz <= 0) {
        for (int i = 0; i < 7; i++) buf7[i] = ' ';
        buf7[6] = (sz == 0) ? '0' : '?';
        return;
    }
    char d[10]; int len = 0;
    for (int n = sz; n > 0; n /= 10) d[len++] = '0' + n % 10;
    if (len > 7) {
        for (int i = 0; i < 6; i++) buf7[i] = ' ';
        buf7[6] = '#';
        return;
    }
    int pad = 7 - len;
    for (int i = 0; i < pad; i++) buf7[i] = ' ';
    for (int i = 0; i < len; i++) buf7[pad + i] = d[len - 1 - i];
}

/* Write a 38-char entry line at (row, pane_col+1). */
static void render_entry(int row, int pane_col, const Entry *e, int fg, int bg) {
    char line[38];
    memset(line, ' ', 38);

    /* Name: left-justified, max 24 chars */
    for (int i = 0; i < 24 && e->name[i]; i++) line[i] = e->name[i];

    /* Type/size at positions 31-37 */
    if (e->is_dir) {
        line[31] = ' '; line[32] = ' ';
        line[33] = '<'; line[34] = 'D'; line[35] = 'I'; line[36] = 'R'; line[37] = '>';
    } else if (e->size >= 0) {
        fmt_size(line + 31, e->size);
    }

    tui_move(row, pane_col + 1);
    tui_color(fg, bg);
    write(STDOUT_FILENO, line, 38);
}

/* Draw the pane frame: outer box + path header + ╠═╣ separator. */
static void draw_pane_frame(int pane) {
    int col  = pane ? RIGHT_COL : LEFT_COL;
    int bfg  = (pane == active) ? C_BORD_ACT : C_BORD_IDLE;
    char ln[40];
    int i;

    tui_color(bfg, C_PANE_BG);

    /* top: ╔═…═╗ */
    ln[0] = '\xC9';
    for (i = 1; i < 39; i++) ln[i] = '\xCD';
    ln[39] = '\xBB';
    tui_move(0, col); write(STDOUT_FILENO, ln, 40);

    /* header row 1: ║ spaces ║ */
    ln[0] = '\xBA';
    for (i = 1; i < 39; i++) ln[i] = ' ';
    ln[39] = '\xBA';
    tui_move(HDR_ROW, col); write(STDOUT_FILENO, ln, 40);

    /* separator row 2: ╠═…═╣ */
    ln[0] = '\xCC';
    for (i = 1; i < 39; i++) ln[i] = '\xCD';
    ln[39] = '\xB9';
    tui_move(SEP_ROW, col); write(STDOUT_FILENO, ln, 40);

    /* content rows 3-21: ║ spaces ║ */
    ln[0] = '\xBA';
    for (i = 1; i < 39; i++) ln[i] = ' ';
    ln[39] = '\xBA';
    for (int r = CONTENT_ROW; r < CONTENT_ROW + CONTENT_H; r++) {
        tui_move(r, col); write(STDOUT_FILENO, ln, 40);
    }

    /* bottom row 22: ╚═…═╝ */
    ln[0] = '\xC8';
    for (i = 1; i < 39; i++) ln[i] = '\xCD';
    ln[39] = '\xBC';
    tui_move(BOTTOM_ROW, col); write(STDOUT_FILENO, ln, 40);

    /* Path text in header row */
    tui_color(TUI_WHITE, C_PANE_BG);
    tui_move(HDR_ROW, col + 1);
    const char *pp = panels[pane].path;
    int plen = 0;
    while (pp[plen] && plen < 37) plen++;
    write(STDOUT_FILENO, pp, plen);
}

/* Redraw just the entry rows of one pane (not the frame). */
static void draw_panel(int pane) {
    int col    = pane ? RIGHT_COL : LEFT_COL;
    int is_act = (pane == active);
    Panel *p   = &panels[pane];
    char blank[38]; memset(blank, ' ', 38);

    for (int r = 0; r < CONTENT_H; r++) {
        int eidx = p->top + r;
        int srow = CONTENT_ROW + r;

        if (eidx >= p->count) {
            tui_move(srow, col + 1);
            tui_color(C_FILE_FG, C_PANE_BG);
            write(STDOUT_FILENO, blank, 38);
            continue;
        }

        Entry *e = &p->ents[eidx];
        int is_cur = (eidx == p->cursor);
        int fg, bg;
        if      (is_cur && is_act) { fg = C_SEL_FG;  bg = C_SEL_BG;  }
        else if (is_cur)           { fg = C_ISEL_FG; bg = C_ISEL_BG; }
        else if (e->is_dir)        { fg = C_DIR_FG;  bg = C_PANE_BG; }
        else                       { fg = C_FILE_FG; bg = C_PANE_BG; }

        render_entry(srow, col, e, fg, bg);
    }
}

static void draw_info(void) {
    tui_fill(INFO_ROW, 0, 1, TUI_COLS, ' ', TUI_BLACK, TUI_CYAN);

    Panel *p = &panels[active];
    if (p->count == 0) return;

    Entry *e = &p->ents[p->cursor];
    char info[78]; int n = 0;

    #define APPEND_CH(ch) do { \
        if (n >= (int)sizeof(info)) goto out; \
        info[n++] = (ch); \
    } while (0)
    #define APPEND_STR(str_lit) do { \
        const char *s__ = (str_lit); \
        while (*s__) { \
            if (n >= (int)sizeof(info)) goto out; \
            info[n++] = *s__++; \
        } \
    } while (0)

    if (strcmp(e->name, "..") == 0) {
        char par[PATH_SZ];
        for (int i = 0; i < PATH_SZ; i++) par[i] = p->path[i];
        path_parent(par);
        APPEND_STR("[UP] ");
        for (int i = 0; par[i]; i++) APPEND_CH(par[i]);
    } else {
        char full[PATH_SZ];
        if (path_join(full, p->path, e->name, PATH_SZ) < 0) {
            APPEND_STR("[path too long]");
            goto out;
        }
        for (int i = 0; full[i]; i++) APPEND_CH(full[i]);
        if (e->is_dir) {
            APPEND_STR("  <dir>");
        } else if (e->size >= 0) {
            APPEND_CH(' '); APPEND_CH(' ');
            char sz[7]; fmt_size(sz, e->size);
            for (int i = 0; i < 7; i++) APPEND_CH(sz[i]);
            APPEND_CH(' '); APPEND_CH('B');
        }
    }

out:
    tui_move(INFO_ROW, 1);
    tui_color(TUI_BLACK, TUI_CYAN);
    write(STDOUT_FILENO, info, n);

    #undef APPEND_STR
    #undef APPEND_CH
}

static void draw_fkeys(void) {
    static const char bar[] =
        "  Tab=Switch  Enter=Open  Bksp=Up  R=Reload  Q=Quit";
    tui_fill(FKEY_ROW, 0, 1, TUI_COLS, ' ', TUI_WHITE, TUI_BLACK);
    tui_move(FKEY_ROW, 0);
    tui_color(TUI_WHITE, TUI_BLACK);
    write(STDOUT_FILENO, bar, sizeof(bar) - 1);
}

static void draw_all(void) {
    draw_pane_frame(0); draw_pane_frame(1);
    draw_panel(0);      draw_panel(1);
    draw_info();        draw_fkeys();
}

/* ── Input ────────────────────────────────────────────────────────────── */

static int read_key(void) {
    char ch;
    if (read(STDIN_FILENO, &ch, 1) != 1) return -1;
    if ((unsigned char)ch != 0x1b) return (unsigned char)ch;

    /* Arrow keys arrive as ESC [ A/B/C/D all in one IRQ burst */
    char ch2;
    if (read(STDIN_FILENO, &ch2, 1) != 1) return -1;
    if (ch2 != '[') return (unsigned char)ch2;   /* ESC + other char */
    char ch3;
    if (read(STDIN_FILENO, &ch3, 1) != 1) return -1;
    switch (ch3) {
    case 'A': return KEY_UP;
    case 'B': return KEY_DOWN;
    case 'C': return KEY_RIGHT;
    case 'D': return KEY_LEFT;
    }
    return 0;
}

/* Show a one-line error in the info bar, wait for any key. */
static void show_error(const char *msg) {
    tui_fill(INFO_ROW, 0, 1, TUI_COLS, ' ', TUI_WHITE, TUI_RED);
    tui_move(INFO_ROW, 1);
    tui_color(TUI_WHITE, TUI_RED);
    int n = 0; while (msg[n]) n++;
    write(STDOUT_FILENO, msg, n);
    char ch; read(STDIN_FILENO, &ch, 1);
    draw_info();
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void) {
    strcpy(panels[0].path, "/disk");
    strcpy(panels[1].path, "/");
    panels[0].cursor = panels[0].top = 0;
    panels[1].cursor = panels[1].top = 0;

    load_panel(&panels[0]);
    load_panel(&panels[1]);

    tui_cursor_hide();
    draw_all();

    for (;;) {
        int k = read_key();
        if (k < 0) break;
        Panel *p = &panels[active];

        switch (k) {

        case KEY_UP:
            if (p->cursor > 0) {
                p->cursor--;
                if (p->cursor < p->top) p->top = p->cursor;
                draw_panel(active); draw_info();
            }
            break;

        case KEY_DOWN:
            if (p->cursor < p->count - 1) {
                p->cursor++;
                if (p->cursor >= p->top + CONTENT_H)
                    p->top = p->cursor - CONTENT_H + 1;
                draw_panel(active); draw_info();
            }
            break;

        /* Tab / left / right: switch active pane */
        case '\t': case KEY_LEFT: case KEY_RIGHT:
            active ^= 1;
            draw_pane_frame(0); draw_pane_frame(1);
            draw_panel(0);      draw_panel(1);
            draw_info();
            break;

        case '\n': case '\r': {
            if (p->count == 0) break;
            Entry *e = &p->ents[p->cursor];
            if (e->is_dir) {
                if (strcmp(e->name, "..") == 0)
                    path_parent(p->path);
                else if (path_join(p->path, p->path, e->name, PATH_SZ) < 0) {
                    show_error("path too long");
                    break;
                }
                p->cursor = 0; p->top = 0;
                load_panel(p);
                draw_pane_frame(active); draw_panel(active); draw_info();
            } else {
                char full[PATH_SZ];
                if (path_join(full, p->path, e->name, PATH_SZ) < 0) {
                    show_error("path too long");
                    break;
                }
                /* Hand the terminal to the child */
                tui_cursor_show(); tui_reset(); tui_clear();
                int pid = exec(full);
                if (pid > 0) {
                    waitpid(pid);
                    tui_cursor_hide();
                    draw_all();
                } else {
                    tui_cursor_hide();
                    draw_all();
                    show_error("exec failed — not a valid ELF");
                }
            }
            break;
        }

        case '\b': case 127: {  /* Backspace — go up one level */
            int at_root = (p->path[0] == '/' && p->path[1] == '\0');
            if (!at_root) {
                path_parent(p->path);
                p->cursor = 0; p->top = 0;
                load_panel(p);
                draw_pane_frame(active); draw_panel(active); draw_info();
            }
            break;
        }

        case 'r': case 'R':
            load_panel(&panels[0]); load_panel(&panels[1]);
            draw_all();
            break;

        case 'q': case 'Q':
            goto done;
        }
    }

done:
    tui_reset(); tui_clear(); tui_cursor_show();
    return 0;
}

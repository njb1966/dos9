#include <keyboard.h>
#include <pic.h>
#include <io.h>
#include <stdint.h>

#define KBD_PORT     0x60
#define KBD_BUF_SIZE 256

/* Scancode set 1 — unshifted */
static const char sc_normal[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\n', 0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
    0,   ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,   0,
    0,   0,    0,   0,   0,   0
};

/* Scancode set 1 — shifted */
static const char sc_shifted[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"',  '~',
    0,   '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
    0,   ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,   0,
    0,   0,    0,   0,   0,   0
};

#define SC_LSHIFT_DN 0x2A
#define SC_LSHIFT_UP 0xAA
#define SC_RSHIFT_DN 0x36
#define SC_RSHIFT_UP 0xB6

static volatile char    kbd_buf[KBD_BUF_SIZE];
static volatile uint8_t kbd_head = 0;   /* write index */
static volatile uint8_t kbd_tail = 0;   /* read index  */
static volatile int     shift    = 0;

static void kbd_irq_handler(struct registers *r) {
    (void)r;
    uint8_t sc = inb(KBD_PORT);

    if (sc == SC_LSHIFT_DN || sc == SC_RSHIFT_DN) { shift = 1; return; }
    if (sc == SC_LSHIFT_UP || sc == SC_RSHIFT_UP) { shift = 0; return; }

    if (sc & 0x80) return;          /* ignore other key-release scancodes */
    if (sc >= 128)  return;

    char ch = shift ? sc_shifted[sc] : sc_normal[sc];
    if (!ch) return;                 /* untranslatable scancode */

    uint8_t next = (kbd_head + 1) & (KBD_BUF_SIZE - 1);
    if (next != kbd_tail) {          /* drop if buffer full */
        kbd_buf[kbd_head] = ch;
        kbd_head = next;
    }
}

void keyboard_init(void) {
    irq_register(IRQ_KEYBOARD, kbd_irq_handler);
}

int kbd_haschar(void) {
    return kbd_head != kbd_tail;
}

char kbd_getchar(void) {
    while (!kbd_haschar())
        __asm__ volatile("sti; hlt");

    char ch = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) & (KBD_BUF_SIZE - 1);
    return ch;
}

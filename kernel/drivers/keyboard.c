#include <keyboard.h>
#include <pic.h>
#include <io.h>
#include <stdint.h>

#define KBD_PORT     0x60
#define KBD_BUF_SIZE 256

/* COM1 serial fallback — lets QEMU -nographic pipe input to the shell */
#define COM1_BASE    0x3F8
static int com1_present;

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

static inline int  com1_has(void)  {
    return com1_present && (inb(COM1_BASE + 5) & 0x01);
}
static inline char com1_get(void)  { return (char)inb(COM1_BASE); }

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
static volatile uint8_t kbd_head  = 0;
static volatile uint8_t kbd_tail  = 0;
static volatile int     shift     = 0;
static volatile int     saw_e0    = 0;   /* extended scancode prefix pending */

static void kbd_push(char ch) {
    uint8_t next = (kbd_head + 1) & (KBD_BUF_SIZE - 1);
    if (next != kbd_tail) {
        kbd_buf[kbd_head] = ch;
        kbd_head = next;
    }
}

static void kbd_irq_handler(struct registers *r) {
    (void)r;
    uint8_t sc = inb(KBD_PORT);

    /* 0xE0 prefix: next scancode is an extended key. */
    if (sc == 0xE0) { saw_e0 = 1; return; }

    /* Extended key: translate arrow keys to ANSI CSI sequences. */
    if (saw_e0) {
        saw_e0 = 0;
        if (sc & 0x80) return;   /* extended key-release — ignore */
        switch (sc) {
            case 0x48: kbd_push('\x1b'); kbd_push('['); kbd_push('A'); break; /* up    */
            case 0x50: kbd_push('\x1b'); kbd_push('['); kbd_push('B'); break; /* down  */
            case 0x4B: kbd_push('\x1b'); kbd_push('['); kbd_push('D'); break; /* left  */
            case 0x4D: kbd_push('\x1b'); kbd_push('['); kbd_push('C'); break; /* right */
        }
        return;
    }

    if (sc == SC_LSHIFT_DN || sc == SC_RSHIFT_DN) { shift = 1; return; }
    if (sc == SC_LSHIFT_UP || sc == SC_RSHIFT_UP) { shift = 0; return; }

    if (sc & 0x80) return;   /* other key-release scancodes */
    if (sc >= 128) return;

    char ch = shift ? sc_shifted[sc] : sc_normal[sc];
    if (!ch) return;

    kbd_push(ch);
}

void keyboard_init(void) {
    com1_present = com1_probe();
    irq_register(IRQ_KEYBOARD, kbd_irq_handler);
}

int kbd_haschar(void) {
    return (kbd_head != kbd_tail) || com1_has();
}

char kbd_getchar(void) {
    while (!kbd_haschar())
        __asm__ volatile("sti; hlt");

    if (kbd_head != kbd_tail) {
        char ch = kbd_buf[kbd_tail];
        kbd_tail = (kbd_tail + 1) & (KBD_BUF_SIZE - 1);
        return ch;
    }
    return com1_get();
}

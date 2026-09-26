#include "kbd.h"
#include "idt.h"
#include "pic.h"
#include "io.h"

#define KBD_BUFFER_SIZE 128

static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

static int shift_pressed = 0;
static int caps_lock = 0;
static volatile unsigned char last_scancode = 0;

static const char kbd_scancode_normal[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char kbd_scancode_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static void kbd_callback(registers_t *regs) {
    (void)regs;
    unsigned char scancode = inb(0x60);
    last_scancode = scancode;

    // Shift press/release
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }
    // Caps lock toggle
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }

    // Ignore key releases (break codes have bit 7 set)
    if (scancode & 0x80) {
        return;
    }

    char ch = 0;
    int is_upper = shift_pressed ^ caps_lock;

    if (shift_pressed) {
        ch = kbd_scancode_shift[scancode];
    } else {
        ch = kbd_scancode_normal[scancode];
        if (is_upper && ch >= 'a' && ch <= 'z') {
            ch -= 32;
        }
    }

    if (ch != 0) {
        int next = (kbd_head + 1) % KBD_BUFFER_SIZE;
        if (next != kbd_tail) {
            kbd_buffer[kbd_head] = ch;
            kbd_head = next;
        }
    }
}

void kbd_init(void) {
    register_interrupt_handler(33, kbd_callback);
    pic_unmask_irq(1);
}

int kbd_has_char(void) {
    return kbd_head != kbd_tail;
}

char kbd_get_char(void) {
    if (kbd_head == kbd_tail) return 0;
    char ch = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return ch;
}

unsigned char kbd_last_scancode(void) {
    return last_scancode;
}

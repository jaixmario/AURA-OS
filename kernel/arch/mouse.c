#include "mouse.h"
#include "idt.h"
#include "pic.h"
#include "io.h"

static int mouse_x = 512;
static int mouse_y = 384;
static int max_x = 1024;
static int max_y = 768;

static unsigned char mouse_cycle = 0;
static signed char mouse_bytes[3];

static int btn_left = 0;
static int btn_right = 0;
static int btn_middle = 0;

static int prev_btn_left = 0;
static int left_clicked = 0;
static int mouse_speed_level = 1; // 0=Slow, 1=Normal, 2=Fast

static inline void mouse_wait_write(void) {
    int timeout = 5000;
    while (timeout--) {
        if ((inb(0x64) & 2) == 0) return;
    }
}

static inline void mouse_wait_read(void) {
    int timeout = 5000;
    while (timeout--) {
        if ((inb(0x64) & 1) == 1) return;
    }
}

static void mouse_write(unsigned char val) {
    mouse_wait_write();
    outb(0x64, 0xD4); // Tell keyboard controller to send next byte to mouse
    mouse_wait_write();
    outb(0x60, val);
}

static unsigned char mouse_read(void) {
    mouse_wait_read();
    return inb(0x60);
}

static void mouse_callback(registers_t *regs) {
    (void)regs;
    unsigned char status = inb(0x64);
    if (!(status & 0x20)) {
        // Not mouse data
        return;
    }

    signed char b = (signed char)inb(0x60);

    if (mouse_cycle == 0) {
        // First byte must have bit 3 set (sync bit)
        if (b & 0x08) {
            mouse_bytes[0] = b;
            mouse_cycle++;
        }
    } else if (mouse_cycle == 1) {
        mouse_bytes[1] = b;
        mouse_cycle++;
    } else if (mouse_cycle == 2) {
        mouse_bytes[2] = b;
        mouse_cycle = 0;

        // Decode packet
        unsigned char flags = (unsigned char)mouse_bytes[0];
        int dx = mouse_bytes[1];
        int dy = mouse_bytes[2];

        // Sign extension
        if (flags & 0x10) dx |= ~0xFF;
        if (flags & 0x20) dy |= ~0xFF;

        // Discard overflow
        if (!(flags & 0xC0)) {
            int abs_dx = (dx < 0) ? -dx : dx;
            int abs_dy = (dy < 0) ? -dy : dy;

            if (mouse_speed_level == 1) { // Normal (1.5x)
                if (abs_dx > 4) dx += (dx > 0) ? (abs_dx - 4) / 2 : -(abs_dx - 4) / 2;
                if (abs_dy > 4) dy += (dy > 0) ? (abs_dy - 4) / 2 : -(abs_dy - 4) / 2;
            } else if (mouse_speed_level == 2) { // Fast (2x)
                dx = (dx * 3) / 2;
                dy = (dy * 3) / 2;
                if (abs_dx > 4) dx += (dx > 0) ? (abs_dx - 4) : -(abs_dx - 4);
                if (abs_dy > 4) dy += (dy > 0) ? (abs_dy - 4) : -(abs_dy - 4);
            } // level 0 is 1x (slow precision)

            mouse_x += dx;
            mouse_y -= dy; // Invert Y because mouse coords go down-up, screen coords up-down

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= max_x) mouse_x = max_x - 1;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= max_y) mouse_y = max_y - 1;
        }

        btn_left   = (flags & 0x01) ? 1 : 0;
        btn_right  = (flags & 0x02) ? 1 : 0;
        btn_middle = (flags & 0x04) ? 1 : 0;

        if (btn_left && !prev_btn_left) {
            left_clicked = 1;
        }
        prev_btn_left = btn_left;
    }
}

void mouse_init(int screen_w, int screen_h) {
    max_x = screen_w;
    max_y = screen_h;
    mouse_x = screen_w / 2;
    mouse_y = screen_h / 2;

    // Enable auxiliary mouse device
    mouse_wait_write();
    outb(0x64, 0xA8);

    // Enable interrupts for mouse (IRQ12)
    mouse_wait_write();
    outb(0x64, 0x20); // Get compo byte
    mouse_wait_read();
    unsigned char status = inb(0x60) | 2; // Enable IRQ12
    mouse_wait_write();
    outb(0x64, 0x60); // Set compo byte
    mouse_wait_write();
    outb(0x60, status);

    // Set default settings
    mouse_write(0xF6);
    mouse_read(); // ACK (0xFA)

    // Set sample rate to 200 Hz (ultra-smooth)
    mouse_write(0xF3);
    mouse_read(); // ACK
    mouse_write(200);
    mouse_read(); // ACK

    // Set resolution to 8 counts/mm
    mouse_write(0xE8);
    mouse_read(); // ACK
    mouse_write(0x03);
    mouse_read(); // ACK

    // Enable data reporting
    mouse_write(0xF4);
    mouse_read(); // ACK (0xFA)

    // Register handler for IRQ12 (INT 44)
    register_interrupt_handler(44, mouse_callback);

    // Unmask IRQ2 (cascade) and IRQ12 (mouse)
    pic_unmask_irq(2);
    pic_unmask_irq(12);
}

int mouse_get_x(void) {
    return mouse_x;
}

int mouse_get_y(void) {
    return mouse_y;
}

int mouse_is_left_down(void) {
    return btn_left;
}

int mouse_is_right_down(void) {
    return btn_right;
}

int mouse_is_middle_down(void) {
    return btn_middle;
}

int mouse_clicked(int btn) {
    if (btn == 0) {
        int c = left_clicked;
        left_clicked = 0;
        return c;
    }
    return 0;
}

void mouse_set_speed(int level) {
    if (level >= 0 && level <= 2) {
        mouse_speed_level = level;
    }
}

int mouse_get_speed(void) {
    return mouse_speed_level;
}

void mouse_set_bounds(int screen_w, int screen_h) {
    max_x = screen_w;
    max_y = screen_h;
    if (mouse_x >= max_x) mouse_x = max_x - 1;
    if (mouse_y >= max_y) mouse_y = max_y - 1;
}

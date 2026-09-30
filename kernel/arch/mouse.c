#include "mouse.h"
#include "kbd.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "io.h"

static int mouse_x = 512;
static int mouse_y = 384;
static int max_x = 1024;
static int max_y = 768;

static unsigned char mouse_cycle = 0;
static signed char mouse_bytes[4];
static int mouse_packet_size = 3;
static unsigned int last_mouse_byte_tick = 0;

static int btn_left = 0;
static int btn_right = 0;
static int btn_middle = 0;

static int prev_btn_left = 0;
static int left_clicked = 0;
static int mouse_speed_level = 1; // 0=Slow, 1=Normal, 2=Fast
static int mouse_detected_flag = 0;

static inline int mouse_wait_write(int timeout) {
    while (timeout--) {
        if ((inb(0x64) & 2) == 0) return 1;
        io_wait();
    }
    return 0;
}

static inline int mouse_wait_read(int timeout) {
    while (timeout--) {
        if ((inb(0x64) & 1) == 1) return 1;
        io_wait();
    }
    return 0;
}

static int mouse_write(unsigned char val) {
    if (!mouse_wait_write(100000)) return 0;
    outb(0x64, 0xD4); // Tell keyboard controller to route next byte to auxiliary (mouse) port
    if (!mouse_wait_write(100000)) return 0;
    outb(0x60, val);
    return 1;
}

static int mouse_read(unsigned char *out_val, int timeout) {
    while (timeout--) {
        unsigned char status = inb(0x64);
        if (status & 1) {
            unsigned char b = inb(0x60);
            if (status & 0x20) {
                // Auxiliary (mouse) data!
                *out_val = b;
                return 1;
            } else {
                // Keyboard scancode arrived on 8042 bus while waiting for mouse response
                kbd_handle_scancode(b);
            }
        }
        io_wait();
    }
    return 0;
}

static int mouse_send_cmd(unsigned char cmd) {
    for (int retry = 0; retry < 3; retry++) {
        if (!mouse_write(cmd)) continue;
        unsigned char ack = 0;
        if (mouse_read(&ack, 100000)) {
            if (ack == 0xFA) return 1;
            if (ack == 0xFE) continue; // Mouse requested resend
        }
    }
    return 0;
}

static void mouse_decode_packet(void) {
    unsigned char flags = (unsigned char)mouse_bytes[0];
    int dx = (int)mouse_bytes[1];
    int dy = (int)mouse_bytes[2];

    // Sign extension according to bit 4 (X sign) and bit 5 (Y sign)
    if (flags & 0x10) dx |= ~0xFF;
    if (flags & 0x20) dy |= ~0xFF;

    // Discard packet if overflow occurred (bit 6 X-overflow, bit 7 Y-overflow)
    if (!(flags & 0xC0)) {
        // Sanity limit on extreme delta jumps
        if (dx > -300 && dx < 300 && dy > -300 && dy < 300) {
            int abs_dx = (dx < 0) ? -dx : dx;
            int abs_dy = (dy < 0) ? -dy : dy;

            if (mouse_speed_level == 1) { // Normal (1.5x with acceleration)
                if (abs_dx > 4) dx += (dx > 0) ? (abs_dx - 4) / 2 : -(abs_dx - 4) / 2;
                if (abs_dy > 4) dy += (dy > 0) ? (abs_dy - 4) / 2 : -(abs_dy - 4) / 2;
            } else if (mouse_speed_level == 2) { // Fast (2x with acceleration)
                dx = (dx * 3) / 2;
                dy = (dy * 3) / 2;
                if (abs_dx > 4) dx += (dx > 0) ? (abs_dx - 4) : -(abs_dx - 4);
                if (abs_dy > 4) dy += (dy > 0) ? (abs_dy - 4) : -(abs_dy - 4);
            }

            mouse_x += dx;
            mouse_y -= dy; // Invert Y because mouse coords go down-up, screen coords up-down

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= max_x) mouse_x = max_x - 1;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= max_y) mouse_y = max_y - 1;
        }
    }

    btn_left   = (flags & 0x01) ? 1 : 0;
    btn_right  = (flags & 0x02) ? 1 : 0;
    btn_middle = (flags & 0x04) ? 1 : 0;

    if (btn_left && !prev_btn_left) {
        left_clicked = 1;
    }
    prev_btn_left = btn_left;
}

void mouse_handle_byte(unsigned char b) {
    unsigned int now = pit_get_ticks();
    if (mouse_cycle != 0 && (now - last_mouse_byte_tick > 15)) {
        // If >150ms elapsed between packet bytes, reset cycle to resync
        mouse_cycle = 0;
    }
    last_mouse_byte_tick = now;

    if (mouse_cycle == 0) {
        // First byte of PS/2 mouse packet must have bit 3 set to 1
        if ((b & 0x08) == 0x08) {
            mouse_bytes[0] = (signed char)b;
            mouse_cycle = 1;
        }
        return;
    }

    if (mouse_cycle == 1) {
        mouse_bytes[1] = (signed char)b;
        mouse_cycle = 2;
        return;
    }

    if (mouse_cycle == 2) {
        mouse_bytes[2] = (signed char)b;
        if (mouse_packet_size == 4) {
            mouse_cycle = 3;
            return;
        }
        mouse_cycle = 0;
        mouse_decode_packet();
        return;
    }

    if (mouse_cycle == 3) {
        mouse_bytes[3] = (signed char)b;
        mouse_cycle = 0;
        mouse_decode_packet();
        return;
    }
}

static void mouse_callback(registers_t *regs) {
    (void)regs;
    unsigned char status = inb(0x64);
    if (!(status & 0x01)) {
        return; // No data available
    }

    // Always read port 0x60 to clear 8042 controller output buffer and de-assert IRQ!
    unsigned char b = inb(0x60);

    if (!(status & 0x20)) {
        // Keyboard data arrived on mouse IRQ line! Route to keyboard driver!
        kbd_handle_scancode(b);
        return;
    }

    mouse_handle_byte(b);
}

void mouse_init(int screen_w, int screen_h) {
    max_x = screen_w;
    max_y = screen_h;
    mouse_x = screen_w / 2;
    mouse_y = screen_h / 2;
    mouse_cycle = 0;
    btn_left = btn_right = btn_middle = 0;
    prev_btn_left = left_clicked = 0;
    mouse_detected_flag = 0;

    // 1. Drain any residual bytes in 8042 controller buffer
    int drain = 1000;
    while (drain-- && (inb(0x64) & 1)) {
        inb(0x60);
        io_wait();
    }

    // 2. Enable auxiliary mouse interface on 8042
    mouse_wait_write(100000);
    outb(0x64, 0xA8);

    // 3. Read & update 8042 command byte
    mouse_wait_write(100000);
    outb(0x64, 0x20); // Command 0x20: Get command byte
    unsigned char status = 0;
    if (mouse_wait_read(100000)) {
        status = inb(0x60);
        status |= 0x02;   // Enable IRQ12
        status |= 0x01;   // Enable IRQ1
        status &= ~0x20;  // Enable mouse clock line (clear disable bit)
        status &= ~0x10;  // Enable keyboard clock line (clear disable bit)
        mouse_wait_write(100000);
        outb(0x64, 0x60); // Command 0x60: Set command byte
        mouse_wait_write(100000);
        outb(0x60, status);
    }

    // 4. Send Set Defaults (0xF6) to place mouse in standard state
    int ok_defaults = mouse_send_cmd(0xF6);

    // 5. Query Device ID (0xF2) to detect standard (3-byte) vs IntelliMouse (4-byte)
    mouse_packet_size = 3;
    if (mouse_send_cmd(0xF2)) {
        unsigned char dev_id = 0;
        if (mouse_read(&dev_id, 100000)) {
            if (dev_id == 3 || dev_id == 4) {
                mouse_packet_size = 4; // 4-byte wheel/extended mouse
            }
        }
    }

    // 6. Send Enable Data Reporting (0xF4)
    int ok_enable = mouse_send_cmd(0xF4);

    // 7. Drain any leftover response bytes
    drain = 100;
    while (drain-- && (inb(0x64) & 1)) {
        inb(0x60);
        io_wait();
    }

    int mouse_detected = (ok_enable || ok_defaults);
    mouse_detected_flag = mouse_detected;

    // Always register mouse callback and unmask IRQ2 (cascade) & IRQ12 (mouse)
    // so any PS/2 mouse or touchpad in VMware or on physical laptops is immediately active!
    register_interrupt_handler(44, mouse_callback);
    pic_unmask_irq(2);  // Slave PIC cascade line
    pic_unmask_irq(12); // Mouse interrupt line
}

int mouse_is_detected(void) {
    return mouse_detected_flag;
}

void mouse_move_relative(int dx, int dy) {
    mouse_x += dx;
    mouse_y += dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= max_x) mouse_x = max_x - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= max_y) mouse_y = max_y - 1;
}

void mouse_center(void) {
    mouse_x = max_x / 2;
    mouse_y = max_y / 2;
}

void mouse_inject_click(int left, int right) {
    if (left) {
        left_clicked = 1;
    }
    btn_left = 0;
    btn_right = 0;
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

#include "pit.h"
#include "idt.h"
#include "pic.h"
#include "kbd.h"
#include "mouse.h"
#include "io.h"

static volatile unsigned int timer_ticks = 0;
static unsigned int timer_freq = 100;
static volatile unsigned int hardware_irq0_count = 0;
static volatile unsigned int tsc_fallback_count = 0;
static volatile unsigned int last_hardware_irq0_tick = 0;

static inline unsigned long long rdtsc(void) {
    unsigned int lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
}

static unsigned long long g_last_tsc = 0;
static unsigned long long g_tsc_at_last_cmos_sec = 0;
static unsigned int g_tsc_khz = 2500000; // 2.5 GHz default estimate
static unsigned long long g_cycles_per_tick = 25000000ULL; // 10ms at 2.5 GHz
static unsigned long long g_cycles_per_frame = 41666666ULL; // 16.6ms at 2.5 GHz
static unsigned long long g_last_frame_tsc = 0;
static unsigned char g_last_cmos_sec = 0xFF;

static void pit_callback(registers_t *regs) {
    (void)regs;
    timer_ticks++;
    hardware_irq0_count++;
    last_hardware_irq0_tick = timer_ticks;
    g_last_tsc = rdtsc();
}

void pit_init(unsigned int frequency) {
    timer_freq = frequency;
    register_interrupt_handler(32, pit_callback);

    unsigned int divisor = 1193180 / frequency;

    // Send the command byte (Channel 0, lo/hi byte, square wave generator)
    outb(PIT_COMMAND, 0x36);

    // Divisor must be sent byte-wise, low byte then high byte
    outb(PIT_CHANNEL0, (unsigned char)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (unsigned char)((divisor >> 8) & 0xFF));

    // Unmask IRQ0
    pic_unmask_irq(0);

    g_last_tsc = rdtsc();
    g_last_frame_tsc = g_last_tsc;
    g_tsc_at_last_cmos_sec = g_last_tsc;

    // Read initial CMOS second
    outb(0x70, 0x00);
    io_wait();
    g_last_cmos_sec = inb(0x71);
}

static unsigned long long g_last_cmos_poll_tsc = 0;

void timer_update_from_tsc(void) {
    unsigned long long now = rdtsc();

    // 1. Dynamic frequency calibration against CMOS RTC seconds (throttled to avoid LPC bus saturation)
    if (g_last_cmos_poll_tsc == 0 || (now - g_last_cmos_poll_tsc > 250000000ULL)) {
        g_last_cmos_poll_tsc = now;
        outb(0x70, 0x00);
        io_wait();
        unsigned char cur_sec = inb(0x71);
        if (g_last_cmos_sec == 0xFF) {
            g_last_cmos_sec = cur_sec;
            g_tsc_at_last_cmos_sec = now;
        } else if (cur_sec != g_last_cmos_sec) {
            unsigned long long delta = now - g_tsc_at_last_cmos_sec;
            if (delta >= 400000000ULL && delta <= 6000000000ULL) {
                g_tsc_khz = (unsigned int)(delta / 1000);
                g_cycles_per_tick = delta / timer_freq;
                g_cycles_per_frame = delta / 60;
            }
            g_last_cmos_sec = cur_sec;
            g_tsc_at_last_cmos_sec = now;
        }
    }

    // 2. Fallback timekeeping if hardware IRQ0 is dormant or not routed by UEFI firmware:
    // If hardware IRQ0 hasn't fired in the last 50ms, advance ticks from invariant TSC!
    if (hardware_irq0_count == 0 || (timer_ticks > last_hardware_irq0_tick + 5)) {
        if (now > g_last_tsc) {
            unsigned long long elapsed = now - g_last_tsc;
            if (elapsed >= g_cycles_per_tick) {
                unsigned int ticks = (unsigned int)(elapsed / g_cycles_per_tick);
                if (ticks > 0) {
                    timer_ticks += ticks;
                    tsc_fallback_count += ticks;
                    g_last_tsc += (unsigned long long)ticks * g_cycles_per_tick;
                }
            }
        } else {
            g_last_tsc = now;
        }
    }
}

void timer_wait_frame_or_input(void) {
    if (g_last_frame_tsc == 0) g_last_frame_tsc = rdtsc();

    while (1) {
        // 1. If keyboard already has a scancode queued, return immediately
        if (kbd_has_char()) {
            break;
        }

        // 2. Poll 8042 controller: if data arrived on 8042 bus, route it!
        // This guarantees mouse and keyboard work even if UEFI masked IRQ1/IRQ12!
        unsigned char st = inb(0x64);
        if (st & 0x01) {
            unsigned char b = inb(0x60);
            if (st & 0x20) {
                mouse_handle_byte(b);
            } else {
                kbd_handle_scancode(b);
            }
            break;
        }

        // 3. Keep TSC timekeeper updated
        timer_update_from_tsc();

        // 4. Check if 16.6ms (1 frame at 60 FPS) has passed
        unsigned long long now = rdtsc();
        if (now - g_last_frame_tsc >= g_cycles_per_frame) {
            break;
        }

        // 5. Low-power CPU pause (guaranteed non-blocking, never halts indefinitely!)
        __asm__ volatile ("pause");
    }
    g_last_frame_tsc = rdtsc();
}

int pit_is_hardware_irq_active(void) {
    return (hardware_irq0_count > 0 && (timer_ticks <= last_hardware_irq0_tick + 10));
}

unsigned int pit_get_hardware_irq0_count(void) {
    return hardware_irq0_count;
}

unsigned int pit_get_tsc_fallback_count(void) {
    return tsc_fallback_count;
}

unsigned int pit_get_tsc_mhz(void) {
    return g_tsc_khz / 1000;
}

unsigned int pit_get_ticks(void) {
    return timer_ticks;
}

unsigned int pit_get_uptime_seconds(void) {
    return timer_ticks / timer_freq;
}

unsigned int pit_get_uptime_ms(void) {
    return (timer_ticks * 1000) / timer_freq;
}

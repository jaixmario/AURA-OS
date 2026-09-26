#include "pit.h"
#include "idt.h"
#include "pic.h"
#include "io.h"

static volatile unsigned int timer_ticks = 0;
static unsigned int timer_freq = 100;

static void pit_callback(registers_t *regs) {
    (void)regs;
    timer_ticks++;
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

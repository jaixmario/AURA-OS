#ifndef PIT_H
#define PIT_H

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

void pit_init(unsigned int frequency);
unsigned int pit_get_ticks(void);
unsigned int pit_get_uptime_seconds(void);
unsigned int pit_get_uptime_ms(void);

// Real-time multi-source timing & laptop anti-freeze subsystem:
void timer_update_from_tsc(void);
void timer_wait_frame_or_input(void);
unsigned int pit_get_hardware_irq0_count(void);
unsigned int pit_get_tsc_fallback_count(void);
unsigned int pit_get_tsc_mhz(void);
int pit_is_hardware_irq_active(void);

#endif

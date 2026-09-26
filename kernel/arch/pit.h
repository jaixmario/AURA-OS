#ifndef PIT_H
#define PIT_H

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

void pit_init(unsigned int frequency);
unsigned int pit_get_ticks(void);
unsigned int pit_get_uptime_seconds(void);
unsigned int pit_get_uptime_ms(void);

#endif

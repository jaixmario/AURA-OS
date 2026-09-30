#include "rtc.h"
#include "io.h"
#include "pit.h"
#include "../libc/string.h"

#define CMOS_ADDRESS  0x70
#define CMOS_DATA     0x71

#define RTC_SEC       0x00
#define RTC_MIN       0x02
#define RTC_HOUR      0x04
#define RTC_DAY       0x07
#define RTC_MONTH     0x08
#define RTC_YEAR      0x09
#define RTC_CENTURY   0x32
#define RTC_STAT_A    0x0A
#define RTC_STAT_B    0x0B

static rtc_time_t current_time = { 0, 0, 12, 26, 9, 2026 };
static unsigned int last_pit_sec = 0;
static unsigned int last_cmos_sync = 0;
static int rtc_initialized = 0;

static unsigned char get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, (unsigned char)reg);
    io_wait();
    return inb(CMOS_DATA);
}

static void set_rtc_register(int reg, unsigned char val) {
    outb(CMOS_ADDRESS, (unsigned char)reg);
    io_wait();
    outb(CMOS_DATA, val);
}

static int get_update_in_progress_flag(void) {
    outb(CMOS_ADDRESS, RTC_STAT_A);
    return (inb(CMOS_DATA) & 0x80);
}

static void wait_for_rtc_update(void) {
    int timeout = 10000;
    while (get_update_in_progress_flag() && --timeout > 0);
}

static unsigned char to_bcd(unsigned int val) {
    return (unsigned char)(((val / 10) << 4) | (val % 10));
}

static int get_days_in_month(unsigned int month, unsigned int year) {
    if (month == 2) {
        int is_leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        return is_leap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }
    return 31;
}

static void advance_seconds(unsigned int secs) {
    while (secs > 0) {
        current_time.second++;
        if (current_time.second >= 60) {
            current_time.second = 0;
            current_time.minute++;
            if (current_time.minute >= 60) {
                current_time.minute = 0;
                current_time.hour++;
                if (current_time.hour >= 24) {
                    current_time.hour = 0;
                    current_time.day++;
                    unsigned int dim = get_days_in_month(current_time.month, current_time.year);
                    if (current_time.day > dim) {
                        current_time.day = 1;
                        current_time.month++;
                        if (current_time.month > 12) {
                            current_time.month = 1;
                            current_time.year++;
                        }
                    }
                }
            }
        }
        secs--;
    }
}

void rtc_sync_from_cmos(void) {
    unsigned char last_sec, last_min, last_hr, last_d, last_mon, last_yr;
    unsigned char sec, min, hr, d, mon, yr;
    unsigned char regB;

    wait_for_rtc_update();
    sec = get_rtc_register(RTC_SEC);
    min = get_rtc_register(RTC_MIN);
    hr  = get_rtc_register(RTC_HOUR);
    d   = get_rtc_register(RTC_DAY);
    mon = get_rtc_register(RTC_MONTH);
    yr  = get_rtc_register(RTC_YEAR);

    int retries = 5;
    do {
        last_sec = sec;
        last_min = min;
        last_hr  = hr;
        last_d   = d;
        last_mon = mon;
        last_yr  = yr;

        wait_for_rtc_update();
        sec = get_rtc_register(RTC_SEC);
        min = get_rtc_register(RTC_MIN);
        hr  = get_rtc_register(RTC_HOUR);
        d   = get_rtc_register(RTC_DAY);
        mon = get_rtc_register(RTC_MONTH);
        yr  = get_rtc_register(RTC_YEAR);
    } while (--retries > 0 &&
             (last_sec != sec || last_min != min || last_hr != hr ||
              last_d != d || last_mon != mon || last_yr != yr));

    regB = get_rtc_register(RTC_STAT_B);

    // Convert BCD to binary if needed
    if (!(regB & 0x04)) {
        sec = ((sec / 16) * 10) + (sec & 0x0F);
        min = ((min / 16) * 10) + (min & 0x0F);
        hr  = (((hr & 0x0F) + (((hr & 0x70) / 16) * 10)) | (hr & 0x80));
        d   = ((d / 16) * 10) + (d & 0x0F);
        mon = ((mon / 16) * 10) + (mon & 0x0F);
        yr  = ((yr / 16) * 10) + (yr & 0x0F);
    }

    // Convert 12 hour to 24 hour if needed
    if (!(regB & 0x02) && (hr & 0x80)) {
        hr = ((hr & 0x7F) + 12) % 24;
    }

    unsigned char century = get_rtc_register(RTC_CENTURY);
    if (!(regB & 0x04) && century != 0) {
        century = ((century / 16) * 10) + (century & 0x0F);
    }

    unsigned int full_year;
    if (century >= 19 && century <= 25) {
        full_year = (century * 100) + yr;
    } else {
        full_year = 2000 + yr;
    }

    // Sanity checks
    if (sec > 59) sec = 0;
    if (min > 59) min = 0;
    if (hr > 23) hr = 0;
    if (mon < 1 || mon > 12) mon = 1;
    if (d < 1 || d > 31) d = 1;
    if (full_year < 2000 || full_year > 2099) full_year = 2026;

    current_time.second = sec;
    current_time.minute = min;
    current_time.hour   = hr;
    current_time.day    = d;
    current_time.month  = mon;
    current_time.year   = full_year;

    last_pit_sec = pit_get_uptime_seconds();
    rtc_initialized = 1;
}

void rtc_init(void) {
    rtc_sync_from_cmos();
}

void rtc_get_datetime(rtc_time_t *out_time) {
    if (!rtc_initialized) {
        rtc_sync_from_cmos();
        last_cmos_sync = pit_get_uptime_seconds();
    }

    unsigned int now_pit = pit_get_uptime_seconds();
    if (now_pit > last_pit_sec) {
        advance_seconds(now_pit - last_pit_sec);
        last_pit_sec = now_pit;
    }

    // Periodic synchronization from hardware CMOS RTC every 30 seconds
    if (now_pit >= last_cmos_sync + 30) {
        rtc_sync_from_cmos();
        last_cmos_sync = now_pit;
    }

    if (out_time) {
        *out_time = current_time;
    }
}

void rtc_set_datetime(const rtc_time_t *new_time) {
    if (!new_time) return;
    current_time = *new_time;
    last_pit_sec = pit_get_uptime_seconds();
    rtc_initialized = 1;

    unsigned char regB = get_rtc_register(RTC_STAT_B);
    int is_bcd = !(regB & 0x04);

    wait_for_rtc_update();
    set_rtc_register(RTC_SEC, is_bcd ? to_bcd(current_time.second % 60) : (current_time.second % 60));
    set_rtc_register(RTC_MIN, is_bcd ? to_bcd(current_time.minute % 60) : (current_time.minute % 60));
    set_rtc_register(RTC_HOUR, is_bcd ? to_bcd(current_time.hour % 24) : (current_time.hour % 24));
    set_rtc_register(RTC_DAY, is_bcd ? to_bcd(current_time.day) : current_time.day);
    set_rtc_register(RTC_MONTH, is_bcd ? to_bcd(current_time.month) : current_time.month);
    unsigned int yr = current_time.year % 100;
    set_rtc_register(RTC_YEAR, is_bcd ? to_bcd(yr) : yr);
}

void rtc_adjust_hour(int delta) {
    rtc_time_t t;
    rtc_get_datetime(&t);
    int h = (int)t.hour + delta;
    while (h < 0) h += 24;
    t.hour = h % 24;
    rtc_set_datetime(&t);
}

void rtc_adjust_minute(int delta) {
    rtc_time_t t;
    rtc_get_datetime(&t);
    int m = (int)t.minute + delta;
    while (m < 0) m += 60;
    t.minute = m % 60;
    t.second = 0;
    rtc_set_datetime(&t);
}

void rtc_adjust_day(int delta) {
    rtc_time_t t;
    rtc_get_datetime(&t);
    int dim = get_days_in_month(t.month, t.year);
    int d = (int)t.day + delta;
    if (d < 1) d = dim;
    if (d > dim) d = 1;
    t.day = d;
    rtc_set_datetime(&t);
}

void rtc_adjust_month(int delta) {
    rtc_time_t t;
    rtc_get_datetime(&t);
    int m = (int)t.month + delta;
    if (m < 1) m = 12;
    if (m > 12) m = 1;
    t.month = m;
    int dim = get_days_in_month(t.month, t.year);
    if (t.day > (unsigned int)dim) t.day = dim;
    rtc_set_datetime(&t);
}

void rtc_adjust_year(int delta) {
    rtc_time_t t;
    rtc_get_datetime(&t);
    int y = (int)t.year + delta;
    if (y >= 2000 && y <= 2099) {
        t.year = y;
        rtc_set_datetime(&t);
    }
}

void rtc_get_time_string(char *buf, unsigned int size) {
    rtc_time_t t;
    rtc_get_datetime(&t);
    snprintf(buf, size, "%02u:%02u:%02u", t.hour, t.minute, t.second);
}

void rtc_get_date_string(char *buf, unsigned int size) {
    rtc_time_t t;
    rtc_get_datetime(&t);
    snprintf(buf, size, "%04u-%02u-%02u", t.year, t.month, t.day);
}

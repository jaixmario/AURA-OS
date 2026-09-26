#ifndef RTC_H
#define RTC_H

typedef struct {
    unsigned int second;
    unsigned int minute;
    unsigned int hour;
    unsigned int day;
    unsigned int month;
    unsigned int year;
} rtc_time_t;

void rtc_init(void);
void rtc_sync_from_cmos(void);
void rtc_get_datetime(rtc_time_t *out_time);
void rtc_set_datetime(const rtc_time_t *new_time);

void rtc_adjust_hour(int delta);
void rtc_adjust_minute(int delta);
void rtc_adjust_day(int delta);
void rtc_adjust_month(int delta);
void rtc_adjust_year(int delta);

void rtc_get_time_string(char *buf, unsigned int size);
void rtc_get_date_string(char *buf, unsigned int size);

#endif

#ifndef RTC_DS3231_H
#define RTC_DS3231_H

#include <stddef.h>
#include <stdbool.h>
#include "app_types.h"

bool ds3231_get_time(rtc_time_t *t);
bool ds3231_set_time(const rtc_time_t *t);
void get_datetime_str(char *out, size_t n);
void get_clock_str(char *out, size_t n);
void rtc_probe_or_init(void);

#endif

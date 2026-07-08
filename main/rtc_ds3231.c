/*
 * main/rtc_ds3231.c
 * ------------------------------------------------------------
 * File này đọc DS3231 qua I2C để tạo thời gian thực cho dữ liệu CSV.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rtc_ds3231.h"
#include "i2c_bus.h"
#include "app_config.h"
#include "app_state.h"

static uint8_t bcd2bin(uint8_t v) { return (uint8_t)(v - 6 * (v >> 4)); }
static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(v + 6 * (v / 10)); }

bool ds3231_get_time(rtc_time_t *t) {
    uint8_t reg = 0x00;
    uint8_t b[7];
    if (tca_select(CH_DS3231) != ESP_OK) return false;
    esp_err_t err = i2c_write_read_bytes(DS3231_ADDR, &reg, 1, b, 7);
    tca_off();
    if (err != ESP_OK) return false;
    t->sec  = bcd2bin(b[0] & 0x7F);
    t->min  = bcd2bin(b[1] & 0x7F);
    t->hour = bcd2bin(b[2] & 0x3F);
    t->day  = bcd2bin(b[4] & 0x3F);
    t->mon  = bcd2bin(b[5] & 0x1F);
    t->year = 2000 + bcd2bin(b[6]);
    return true;
}

bool ds3231_set_time(const rtc_time_t *t) {
    uint8_t b[8];
    b[0] = 0x00;
    b[1] = bin2bcd(t->sec);
    b[2] = bin2bcd(t->min);
    b[3] = bin2bcd(t->hour);
    b[4] = bin2bcd(1);
    b[5] = bin2bcd(t->day);
    b[6] = bin2bcd(t->mon);
    b[7] = bin2bcd(t->year - 2000);
    if (tca_select(CH_DS3231) != ESP_OK) return false;
    esp_err_t err = i2c_write_bytes(DS3231_ADDR, b, sizeof(b));
    tca_off();
    return err == ESP_OK;
}

void get_datetime_str(char *out, size_t n) {
    rtc_time_t t;
    if (!rtcReady || !ds3231_get_time(&t)) {
        snprintf(out, n, "0000-00-00 00:00:00");
        return;
    }
    snprintf(out, n, "%04d-%02d-%02d %02d:%02d:%02d", t.year, t.mon, t.day, t.hour, t.min, t.sec);
}

void get_clock_str(char *out, size_t n) {
    rtc_time_t t;
    if (!rtcReady || !ds3231_get_time(&t)) {
        snprintf(out, n, "RTC ERR");
        return;
    }
    snprintf(out, n, "%02d/%02d %02d:%02d:%02d", t.day, t.mon, t.hour, t.min, t.sec);
}

void rtc_probe_or_init(void) {
    rtc_time_t t;
    rtcReady = ds3231_get_time(&t);
    if (!rtcReady) {
        rtc_time_t init = {2026, 1, 1, 0, 0, 0};
        rtcReady = ds3231_set_time(&init);
    }
}

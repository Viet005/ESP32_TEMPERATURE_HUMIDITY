/*
 * main/ui_menu.c
 * ------------------------------------------------------------
 * File này xử lý 4 nút nhấn, đổi trang LCD và vẽ nội dung menu. Logic được giữ kiểu Arduino .ino để menu mượt và dễ hiểu.
 */

#include <stdio.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_menu.h"
#include "lcd2004.h"
#include "rtc_ds3231.h"
#include "storage_sd.h"
#include "app_config.h"
#include "app_state.h"

void init_buttons(void) {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL<<BTN_UP) | (1ULL<<BTN_DOWN) | (1ULL<<BTN_OK) | (1ULL<<BTN_BACK),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
}

void draw_page(void) {
    sensor_data_t dht, sht, htu, a10, a20;
    sds_data_t sds;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        dht = dhtData; sht = shtData; htu = htuData; a10 = aht10Data; a20 = aht20Data; sds = sdsData;
        xSemaphoreGive(dataMutex);
    } else return;

    char line[64], v1[16], v2[16];
    if (currentPage != lastDrawnPage) {
        lcd_clear();
        lastDrawnPage = currentPage;
    }
    switch (currentPage) {
        case PAGE_HOME: {
            char clk[32]; get_clock_str(clk, sizeof(clk));
            lcd_print20(0, clk);
            snprintf(line, sizeof(line), "Meas:%s Log:%s", on_off(measuring), on_off(autoLog)); lcd_print20(1, line);
            lcd_print20(2, "IP:192.168.4.1");
            lcd_print20(3, "UP/DN Page OK Ctrl");
            break;
        }
        case PAGE_SENSOR_1:
            lcd_print20(0, "DHT / SHT / HTU");
            ftoa_or_na(v1, sizeof(v1), dht.t, dht.ok, 1); ftoa_or_na(v2, sizeof(v2), dht.h, dht.ok, 1);
            snprintf(line, sizeof(line), "DHT %sC %s%%", v1, v2); lcd_print20(1, line);
            ftoa_or_na(v1, sizeof(v1), sht.t, sht.ok, 1); ftoa_or_na(v2, sizeof(v2), sht.h, sht.ok, 1);
            snprintf(line, sizeof(line), "SHT %sC %s%%", v1, v2); lcd_print20(2, line);
            ftoa_or_na(v1, sizeof(v1), htu.t, htu.ok, 1); ftoa_or_na(v2, sizeof(v2), htu.h, htu.ok, 1);
            snprintf(line, sizeof(line), "HTU %sC %s%%", v1, v2); lcd_print20(3, line);
            break;
        case PAGE_SENSOR_2:
            lcd_print20(0, "AHT10 / AHT20");
            ftoa_or_na(v1, sizeof(v1), a10.t, a10.ok, 1); ftoa_or_na(v2, sizeof(v2), a10.h, a10.ok, 1);
            snprintf(line, sizeof(line), "A10 %sC %s%%", v1, v2); lcd_print20(1, line);
            ftoa_or_na(v1, sizeof(v1), a20.t, a20.ok, 1); ftoa_or_na(v2, sizeof(v2), a20.h, a20.ok, 1);
            snprintf(line, sizeof(line), "A20 %sC %s%%", v1, v2); lcd_print20(2, line);
            lcd_print20(3, "5 phut / 1 mau");
            break;
        case PAGE_DUST:
            lcd_print20(0, "SDS011");
            ftoa_or_na(v1, sizeof(v1), sds.pm25, sds.ok, 1);
            snprintf(line, sizeof(line), "PM2.5:%s ug/m3", v1); lcd_print20(1, line);
            ftoa_or_na(v1, sizeof(v1), sds.pm10, sds.ok, 1);
            snprintf(line, sizeof(line), "PM10 :%s ug/m3", v1); lcd_print20(2, line);
            snprintf(line, sizeof(line), "OK:%d", sds.ok ? 1 : 0); lcd_print20(3, line);
            break;
        case PAGE_STATUS:
            snprintf(line, sizeof(line), "Saved:%lu", (unsigned long)savedRows); lcd_print20(0, line);
            snprintf(line, sizeof(line), "File:%s", currentSessionFile); lcd_print20(1, line);
            snprintf(line, sizeof(line), "SD:%s", sdReady ? "OK" : "ERR"); lcd_print20(2, line);
            lcd_print20(3, "WEB 192.168.4.1");
            break;
        case PAGE_CONTROL:
            lcd_print20(0, "CONTROL");
            snprintf(line, sizeof(line), "%cMeasure:%s", controlIndex == 0 ? '>' : ' ', measuring ? "STOP" : "START"); lcd_print20(1, line);
            snprintf(line, sizeof(line), "%cLog SD:%s", controlIndex == 1 ? '>' : ' ', on_off(autoLog)); lcd_print20(2, line);
            snprintf(line, sizeof(line), "%cLCD Light:%s", controlIndex == 2 ? '>' : ' ', on_off(lcdLightOn)); lcd_print20(3, line);
            break;
        default: break;
    }
}

static bool button_pressed(gpio_num_t pin) {
    static int64_t lastButtonUs = 0;
    if ((now_us() - lastButtonUs) < 180000) return false;
    if (gpio_get_level(pin) == 0) {
        esp_rom_delay_us(15000);
        if (gpio_get_level(pin) == 0) {
            lastButtonUs = now_us();
            while (gpio_get_level(pin) == 0) vTaskDelay(pdMS_TO_TICKS(5));
            return true;
        }
    }
    return false;
}

void handle_buttons_ino_style(void) {
    if (button_pressed(BTN_UP)) {
        if (currentPage == PAGE_CONTROL) {
            controlIndex--;
            if (controlIndex < 0) controlIndex = 2;
        } else {
            currentPage = (currentPage == 0) ? PAGE_STATUS : (ui_page_t)(currentPage - 1);
        }
        uiDirty = true;
    }

    if (button_pressed(BTN_DOWN)) {
        if (currentPage == PAGE_CONTROL) {
            controlIndex++;
            if (controlIndex > 2) controlIndex = 0;
        } else {
            currentPage = (ui_page_t)(currentPage + 1);
            if (currentPage >= PAGE_CONTROL) currentPage = PAGE_HOME;
        }
        uiDirty = true;
    }

    if (button_pressed(BTN_OK)) {
        if (currentPage == PAGE_CONTROL) {
            if (controlIndex == 0) {
                if (measuring) stop_measurement();
                else start_measurement();
            } else if (controlIndex == 1) {
                autoLog = !autoLog;
                save_state();
            } else if (controlIndex == 2) {
                lcdLightOn = !lcdLightOn;
                lcd_apply_backlight();
                save_state();
            }
        } else {
            currentPage = PAGE_CONTROL;
            controlIndex = 0;
        }
        uiDirty = true;
    }

    if (button_pressed(BTN_BACK)) {
        currentPage = PAGE_HOME;
        uiDirty = true;
    }
}

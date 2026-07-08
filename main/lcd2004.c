/*
 * main/lcd2004.c
 * ------------------------------------------------------------
 * File này tự điều khiển LCD2004 qua module I2C PCF8574, không dùng thư viện LiquidCrystal_I2C.
 */

#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "lcd2004.h"
#include "i2c_bus.h"
#include "app_config.h"
#include "app_state.h"

#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RS        0x01

static uint8_t lcd_bl(void) { return lcdLightOn ? LCD_BACKLIGHT : 0; }

static void lcd_expander_write(uint8_t data) {
    uint8_t d = data | lcd_bl();
    i2c_write_bytes(LCD_ADDR, &d, 1);
}

static void lcd_pulse_enable(uint8_t data) {
    lcd_expander_write(data | LCD_ENABLE);
    esp_rom_delay_us(1);
    lcd_expander_write(data & ~LCD_ENABLE);
    esp_rom_delay_us(50);
}

static void lcd_write4(uint8_t nibble, bool rs) {
    uint8_t data = (nibble & 0xF0) | (rs ? LCD_RS : 0);
    lcd_pulse_enable(data);
}

static void lcd_send(uint8_t val, bool rs) {
    lcd_write4(val & 0xF0, rs);
    lcd_write4((val << 4) & 0xF0, rs);
}

static void lcd_cmd(uint8_t cmd) { lcd_send(cmd, false); }
static void lcd_data(uint8_t data) { lcd_send(data, true); }

static void lcd_set_cursor(uint8_t col, uint8_t row) {
    static const uint8_t offsets[] = {0x00, 0x40, 0x14, 0x54};
    lcd_cmd(0x80 | (col + offsets[row % 4]));
}

void lcd_clear(void) {
    lcd_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd_init(void) {
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_write4(0x30, false); vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write4(0x30, false); esp_rom_delay_us(150);
    lcd_write4(0x30, false); esp_rom_delay_us(150);
    lcd_write4(0x20, false); esp_rom_delay_us(150);
    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_clear();
}

void lcd_print20(uint8_t row, const char *text) {
    char buf[21];
    memset(buf, ' ', 20);
    buf[20] = 0;
    size_t len = strlen(text);
    if (len > 20) len = 20;
    memcpy(buf, text, len);
    lcd_set_cursor(0, row);
    for (int i = 0; i < 20; i++) lcd_data((uint8_t)buf[i]);
}

void lcd_apply_backlight(void) {
    lcd_expander_write(0x00);
}

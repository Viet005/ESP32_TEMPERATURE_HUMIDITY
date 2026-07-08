/*
 * main/i2c_bus.c
 * ------------------------------------------------------------
 * File này khởi tạo I2C driver mới của ESP-IDF v6 và cung cấp hàm chọn kênh TCA9548A.
 */

#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "i2c_bus.h"
#include "app_config.h"
#include "app_state.h"

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t dev_tca = NULL;
static i2c_master_dev_handle_t dev_lcd = NULL;
static i2c_master_dev_handle_t dev_sht31 = NULL;
static i2c_master_dev_handle_t dev_htu21d = NULL;
static i2c_master_dev_handle_t dev_aht = NULL;
static i2c_master_dev_handle_t dev_ds3231 = NULL;

static esp_err_t i2c_add_device(uint8_t addr, i2c_master_dev_handle_t *out_dev) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    return i2c_master_bus_add_device(i2c_bus, &dev_cfg, out_dev);
}

static i2c_master_dev_handle_t i2c_get_dev(uint8_t addr) {
    switch (addr) {
        case TCA_ADDR: return dev_tca;
        case LCD_ADDR: return dev_lcd;
        case SHT31_ADDR: return dev_sht31;
        case HTU21D_ADDR: return dev_htu21d;
        case AHT_ADDR: return dev_aht;
        case DS3231_ADDR: return dev_ds3231;
        default: return NULL;
    }
}

void i2c_bus_init(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));
    ESP_ERROR_CHECK(i2c_add_device(TCA_ADDR, &dev_tca));
    ESP_ERROR_CHECK(i2c_add_device(LCD_ADDR, &dev_lcd));
    ESP_ERROR_CHECK(i2c_add_device(SHT31_ADDR, &dev_sht31));
    ESP_ERROR_CHECK(i2c_add_device(HTU21D_ADDR, &dev_htu21d));
    ESP_ERROR_CHECK(i2c_add_device(AHT_ADDR, &dev_aht));
    ESP_ERROR_CHECK(i2c_add_device(DS3231_ADDR, &dev_ds3231));
}

esp_err_t i2c_write_bytes(uint8_t addr, const uint8_t *data, size_t len) {
    i2c_master_dev_handle_t dev = i2c_get_dev(addr);
    if (!dev) return ESP_ERR_INVALID_ARG;
    return i2c_master_transmit(dev, data, len, 100);
}

esp_err_t i2c_read_bytes(uint8_t addr, uint8_t *data, size_t len) {
    i2c_master_dev_handle_t dev = i2c_get_dev(addr);
    if (!dev) return ESP_ERR_INVALID_ARG;
    return i2c_master_receive(dev, data, len, 100);
}

esp_err_t i2c_write_read_bytes(uint8_t addr, const uint8_t *w, size_t wlen, uint8_t *r, size_t rlen) {
    i2c_master_dev_handle_t dev = i2c_get_dev(addr);
    if (!dev) return ESP_ERR_INVALID_ARG;
    return i2c_master_transmit_receive(dev, w, wlen, r, rlen, 100);
}

esp_err_t tca_select(uint8_t ch) {
    uint8_t b = (uint8_t)(1 << ch);
    return i2c_write_bytes(TCA_ADDR, &b, 1);
}

void tca_off(void) {
    uint8_t b = 0x00;
    i2c_write_bytes(TCA_ADDR, &b, 1);
}

void recover_i2c_bus(void) {
    ESP_LOGW(TAG, "I2C recover requested");
    tca_off();
}

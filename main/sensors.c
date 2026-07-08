/*
 * main/sensors.c
 * ------------------------------------------------------------
 * File này đọc DHT11 và các cảm biến I2C qua TCA9548A. Dữ liệu sau khi đọc được ghi vào biến toàn cục trong app_state.
 */

#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sensors.h"
#include "i2c_bus.h"
#include "rtc_ds3231.h"
#include "app_config.h"
#include "app_state.h"

static uint8_t crc8_sensirion(const uint8_t *data, int len) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

void init_dht_pin(void) {
    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);
    gpio_set_level(DHT_PIN, 1);
}

static int dht_wait_until_level(gpio_num_t pin, int target_level, int timeout_us) {
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(pin) != target_level) {
        if ((esp_timer_get_time() - start) > timeout_us) return -1;
    }
    return (int)(esp_timer_get_time() - start);
}

static int dht_measure_level_us(gpio_num_t pin, int level, int timeout_us) {
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(pin) == level) {
        if ((esp_timer_get_time() - start) > timeout_us) return -1;
    }
    return (int)(esp_timer_get_time() - start);
}

static sensor_data_t read_dht11_once(void) {
    sensor_data_t d = invalid_sensor();
    uint8_t data[5] = {0};

    gpio_set_direction(DHT_PIN, GPIO_MODE_OUTPUT_OD);
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);
    gpio_set_level(DHT_PIN, 0);
    esp_rom_delay_us(20000);

    portENTER_CRITICAL(&dhtMux);

    gpio_set_level(DHT_PIN, 1);
    esp_rom_delay_us(35);
    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);

    if (dht_wait_until_level(DHT_PIN, 0, 120) < 0) goto fail;
    if (dht_measure_level_us(DHT_PIN, 0, 140) < 0) goto fail;
    if (dht_measure_level_us(DHT_PIN, 1, 140) < 0) goto fail;

    for (int i = 0; i < 40; i++) {
        if (dht_measure_level_us(DHT_PIN, 0, 90) < 0) goto fail;
        int high_us = dht_measure_level_us(DHT_PIN, 1, 120);
        if (high_us < 0) goto fail;
        data[i / 8] <<= 1;
        if (high_us > 45) data[i / 8] |= 1;
    }

    portEXIT_CRITICAL(&dhtMux);

    uint8_t sum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (sum != data[4]) return d;

    d.h = (float)data[0] + data[1] / 10.0f;
    d.t = (float)data[2] + data[3] / 10.0f;
    d.ok = true;
    return d;

fail:
    portEXIT_CRITICAL(&dhtMux);
    return d;
}

static sensor_data_t read_dht11(void) {
    sensor_data_t d = read_dht11_once();
    if (d.ok) return d;
    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);
    esp_rom_delay_us(30000);
    return read_dht11_once();
}

static bool sht31_probe(void) {
    if (tca_select(CH_SHT31) != ESP_OK) return false;
    uint8_t cmd[2] = {0x30, 0xA2};
    esp_err_t err = i2c_write_bytes(SHT31_ADDR, cmd, 2);
    tca_off();
    return err == ESP_OK;
}

static sensor_data_t read_sht31(void) {
    sensor_data_t d = invalid_sensor();
    uint8_t cmd[2] = {0x24, 0x00};
    uint8_t b[6];

    for (int attempt = 0; attempt < 3; attempt++) {
        if (tca_select(CH_SHT31) != ESP_OK) return d;
        esp_err_t err = i2c_write_bytes(SHT31_ADDR, cmd, 2);
        if (err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));
            err = i2c_read_bytes(SHT31_ADDR, b, 6);
        }
        tca_off();
        if (err == ESP_OK && crc8_sensirion(&b[0], 2) == b[2] && crc8_sensirion(&b[3], 2) == b[5]) {
            uint16_t rawT = ((uint16_t)b[0] << 8) | b[1];
            uint16_t rawH = ((uint16_t)b[3] << 8) | b[4];
            d.t = -45.0f + 175.0f * ((float)rawT / 65535.0f);
            d.h = 100.0f * ((float)rawH / 65535.0f);
            d.ok = true;
            return d;
        }
    }
    shtInitOK = sht31_probe();
    return d;
}

static bool htu21d_probe(void) {
    if (tca_select(CH_HTU21D) != ESP_OK) return false;
    uint8_t cmd = 0xFE;
    esp_err_t err = i2c_write_bytes(HTU21D_ADDR, &cmd, 1);
    tca_off();
    vTaskDelay(pdMS_TO_TICKS(20));
    return err == ESP_OK;
}

static bool htu_read_raw(uint8_t cmd, uint16_t *raw, int wait_ms) {
    uint8_t b[3];
    esp_err_t err = i2c_write_bytes(HTU21D_ADDR, &cmd, 1);
    if (err != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
    err = i2c_read_bytes(HTU21D_ADDR, b, 3);
    if (err != ESP_OK) return false;
    if (crc8_sensirion(b, 2) != b[2]) ESP_LOGW(TAG, "HTU21D CRC warning");
    *raw = (((uint16_t)b[0] << 8) | b[1]) & 0xFFFC;
    return true;
}

static sensor_data_t read_htu21d(void) {
    sensor_data_t d = invalid_sensor();
    for (int attempt = 0; attempt < 3; attempt++) {
        if (tca_select(CH_HTU21D) != ESP_OK) return d;
        uint16_t rt = 0, rh = 0;
        bool okT = htu_read_raw(0xF3, &rt, 50);
        bool okH = htu_read_raw(0xF5, &rh, 50);
        tca_off();
        if (okT && okH) {
            d.t = -46.85f + 175.72f * ((float)rt / 65536.0f);
            d.h = -6.0f + 125.0f * ((float)rh / 65536.0f);
            if (d.h < 0) d.h = 0;
            if (d.h > 100) d.h = 100;
            d.ok = true;
            return d;
        }
        htu21d_probe();
    }
    htuInitOK = htu21d_probe();
    return d;
}

static bool aht_init_channel(uint8_t ch) {
    if (tca_select(ch) != ESP_OK) return false;
    uint8_t init_cmd[3] = {0xBE, 0x08, 0x00};
    esp_err_t err = i2c_write_bytes(AHT_ADDR, init_cmd, 3);
    tca_off();
    vTaskDelay(pdMS_TO_TICKS(20));
    return err == ESP_OK;
}

static sensor_data_t read_aht_channel(uint8_t ch, bool *initOK) {
    sensor_data_t d = invalid_sensor();
    if (!*initOK) {
        *initOK = aht_init_channel(ch);
        return d;
    }
    for (int attempt = 0; attempt < 3; attempt++) {
        if (tca_select(ch) != ESP_OK) return d;
        uint8_t trig[3] = {0xAC, 0x33, 0x00};
        esp_err_t err = i2c_write_bytes(AHT_ADDR, trig, 3);
        if (err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(80));
            uint8_t b[6];
            err = i2c_read_bytes(AHT_ADDR, b, 6);
            if (err == ESP_OK && (b[0] & 0x80) == 0) {
                uint32_t rawH = ((uint32_t)b[1] << 12) | ((uint32_t)b[2] << 4) | ((b[3] >> 4) & 0x0F);
                uint32_t rawT = (((uint32_t)b[3] & 0x0F) << 16) | ((uint32_t)b[4] << 8) | b[5];
                d.h = ((float)rawH * 100.0f) / 1048576.0f;
                d.t = ((float)rawT * 200.0f) / 1048576.0f - 50.0f;
                d.ok = true;
                tca_off();
                return d;
            }
        }
        tca_off();
        *initOK = aht_init_channel(ch);
    }
    return d;
}

void sensors_probe_all(void) {
    shtInitOK = sht31_probe();
    htuInitOK = htu21d_probe();
    aht10InitOK = aht_init_channel(CH_AHT10);
    aht20InitOK = aht_init_channel(CH_AHT20);
    rtc_probe_or_init();
    ESP_LOGI(TAG, "Probe: SHT=%d HTU=%d AHT10=%d AHT20=%d RTC=%d", shtInitOK, htuInitOK, aht10InitOK, aht20InitOK, rtcReady);
}

void read_all_sensors(void) {
    sensor_data_t dht = read_dht11();
    sensor_data_t sht = shtInitOK ? read_sht31() : invalid_sensor();
    sensor_data_t htu = htuInitOK ? read_htu21d() : invalid_sensor();
    sensor_data_t a10 = read_aht_channel(CH_AHT10, &aht10InitOK);
    sensor_data_t a20 = read_aht_channel(CH_AHT20, &aht20InitOK);

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        dhtData = dht;
        shtData = sht;
        htuData = htu;
        aht10Data = a10;
        aht20Data = a20;
        xSemaphoreGive(dataMutex);
    }
    if (!sht.ok || !htu.ok || !a10.ok || !a20.ok) recover_i2c_bus();
}

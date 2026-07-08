/*
 * main/sds011.c
 * ------------------------------------------------------------
 * File này đọc frame UART của SDS011, kiểm tra checksum rồi lấy PM2.5 và PM10.
 */

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "sds011.h"
#include "app_config.h"
#include "app_state.h"

void init_sds_uart(void) {
    uart_config_t cfg = {
        .baud_rate = SDS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(SDS_UART, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(SDS_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(SDS_UART, SDS_TX, SDS_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void sds_task(void *arg) {
    uint8_t frame[10];
    int idx = 0;
    while (1) {
        uint8_t b;
        int n = uart_read_bytes(SDS_UART, &b, 1, pdMS_TO_TICKS(100));
        if (n <= 0) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (sdsData.ok && (now_us() - sdsData.last_frame_us) > (int64_t)SDS_TIMEOUT_MS * 1000) {
                    sdsData.ok = false;
                }
                xSemaphoreGive(dataMutex);
            }
            continue;
        }
        if (idx == 0 && b != 0xAA) continue;
        frame[idx++] = b;
        if (idx < 10) continue;
        idx = 0;
        if (frame[0] != 0xAA || frame[1] != 0xC0 || frame[9] != 0xAB) continue;
        uint8_t checksum = 0;
        for (int i = 2; i <= 7; i++) checksum += frame[i];
        if (checksum != frame[8]) continue;

        float pm25 = (((uint16_t)frame[3] << 8) | frame[2]) / 10.0f;
        float pm10 = (((uint16_t)frame[5] << 8) | frame[4]) / 10.0f;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            sdsData.pm25 = pm25;
            sdsData.pm10 = pm10;
            sdsData.ok = true;
            sdsData.last_frame_us = now_us();
            xSemaphoreGive(dataMutex);
        }
    }
}

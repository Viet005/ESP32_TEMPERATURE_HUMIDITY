/*
 * app_main.c
 * ------------------------------------------------------------
 * Đây là file chính của chương trình ESP-IDF.
 * Cách tổ chức cố ý giống code Arduino .ino ổn định trước đó:
 * - một vòng lặp chính kiểm tra nút, đọc cảm biến, ghi SD, cập nhật LCD;
 * - SDS011 chạy task riêng vì chỉ dùng UART, không tranh I2C với LCD/cảm biến.
 */

#include <math.h>                 // NAN cho giá trị lỗi cảm biến
#include "freertos/FreeRTOS.h"    // FreeRTOS base
#include "freertos/task.h"        // xTaskCreate, vTaskDelay
#include "nvs_flash.h"            // nvs_flash_init
#include "esp_err.h"              // ESP_ERROR_CHECK
#include "esp_log.h"              // log ESP-IDF

#include "app_state.h"            // biến trạng thái toàn cục
#include "i2c_bus.h"              // init I2C + TCA
#include "lcd2004.h"              // LCD2004 I2C
#include "sensors.h"              // DHT11, SHT31, HTU21D, AHT10, AHT20
#include "sds011.h"               // SDS011 UART
#include "storage_sd.h"           // SD + CSV + session
#include "ui_menu.h"              // menu LCD + nút nhấn
#include "web_server.h"           // Wi-Fi AP + HTTP server

/* Task vòng lặp chính, tương đương loop() trong Arduino. */
static void main_loop_task(void *arg) {
    int64_t lastSensorUs = now_us();                            // mốc thời gian lần đọc cảm biến gần nhất
    int64_t lastLcdUs = now_us();                               // mốc thời gian lần refresh LCD gần nhất
    int64_t lastLogUs = now_us();                               // mốc thời gian lần ghi CSV gần nhất

    while (1) {                                                  // vòng lặp chạy mãi
        handle_buttons_ino_style();                             // đọc nút theo kiểu bấm-nhả, chống loạn menu

        if (elapsed_ms(lastSensorUs, SENSOR_PERIOD_MS)) {        // nếu đã tới chu kỳ đọc cảm biến
            read_all_sensors();                                  // đọc 5 cảm biến nhiệt ẩm qua DHT/I2C
            lastSensorUs = now_us();                             // cập nhật mốc thời gian đọc cảm biến
            uiDirty = true;                                      // báo LCD cần cập nhật dữ liệu mới
        }

        if (measuring && elapsed_ms(lastLogUs, LOG_PERIOD_MS)) {  // nếu đang đo và đủ 5 phút
            read_all_sensors();                                  // đọc lại ngay trước khi ghi để lấy mẫu mới nhất
            append_session_row();                                // ghi một dòng CSV vào thẻ SD
            lastLogUs = now_us();                                // cập nhật mốc thời gian ghi
            uiDirty = true;                                      // cập nhật số dòng lưu trên LCD
        }

        if (uiDirty || elapsed_ms(lastLcdUs, LCD_PERIOD_MS)) {    // nếu có thay đổi hoặc tới chu kỳ refresh LCD
            draw_page();                                         // vẽ lại trang hiện tại của LCD
            uiDirty = false;                                     // đã vẽ xong nên xóa cờ dirty
            lastLcdUs = now_us();                                // cập nhật mốc thời gian LCD
        }

        vTaskDelay(pdMS_TO_TICKS(10));                           // nhường CPU 10 ms cho Wi-Fi, web, FreeRTOS
    }
}

/* app_main là điểm bắt đầu chương trình trong ESP-IDF. */
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());                           // khởi tạo NVS để lưu trạng thái đo
    dataMutex = xSemaphoreCreateMutex();                         // tạo mutex bảo vệ dữ liệu cảm biến dùng chung

    dhtData = invalid_sensor();                                  // khởi tạo DHT11 ở trạng thái lỗi/NA
    shtData = invalid_sensor();                                  // khởi tạo SHT31
    htuData = invalid_sensor();                                  // khởi tạo HTU21D
    aht10Data = invalid_sensor();                                // khởi tạo AHT10
    aht20Data = invalid_sensor();                                // khởi tạo AHT20
    sdsData.pm25 = NAN;                                          // PM2.5 ban đầu chưa có dữ liệu
    sdsData.pm10 = NAN;                                          // PM10 ban đầu chưa có dữ liệu
    sdsData.ok = false;                                          // SDS011 ban đầu chưa OK
    sdsData.last_frame_us = 0;                                   // chưa nhận frame SDS011 nào

    load_state();                                                // đọc trạng thái cũ từ NVS

    init_buttons();                                              // cấu hình 4 nút nhấn với pull-up nội
    init_dht_pin();                                              // chuẩn bị chân DHT11 GPIO4
    i2c_bus_init();                                              // khởi tạo bus I2C chính GPIO21/GPIO22

    lcd_init();                                                  // khởi tạo LCD2004 I2C
    lcd_print20(0, "ESP32 IDF LOGGER");                         // dòng 1 thông báo khởi động
    lcd_print20(1, "Init system...");                           // dòng 2 thông báo đang init

    init_sds_uart();                                             // khởi tạo UART2 cho SDS011 GPIO16/GPIO17
    sensors_probe_all();                                         // thử khởi tạo/probe các cảm biến I2C qua TCA

    lcd_print20(2, "Init SD CS=GPIO5");                         // báo đang khởi tạo SD, CS đã là GPIO5
    init_sd();                                                   // khởi tạo microSD qua SPI

    lcd_print20(3, "Init WiFi/Web...");                         // báo đang khởi tạo Wi-Fi AP và web
    wifi_init_ap();                                              // ESP32 phát Wi-Fi ESP32-LOGGER
    start_webserver();                                           // tạo web server http://192.168.4.1

    resume_session_if_needed();                                  // nếu trước đó đang đo thì nối tiếp file cũ
    read_all_sensors();                                          // đọc cảm biến lần đầu để LCD/web có dữ liệu
    draw_page();                                                 // vẽ giao diện LCD lần đầu

    xTaskCreate(main_loop_task, "main_loop_task", 8192, NULL, 5, NULL); // tạo task vòng lặp chính
    xTaskCreate(sds_task, "sds_task", 4096, NULL, 4, NULL);             // tạo task đọc SDS011 liên tục bằng UART
}

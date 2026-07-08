#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/*
 * app_config.h
 * ------------------------------------------------------------
 * File này là nơi gom toàn bộ cấu hình phần cứng của hệ thống.
 * Khi cần đổi chân ESP32, đổi kênh TCA hoặc đổi chu kỳ đo/lưu,
 * chỉ nên sửa ở file này để các file .c khác không phải sửa lại.
 */

/* Header GPIO của ESP-IDF: dùng kiểu gpio_num_t như GPIO_NUM_5. */
#include "driver/gpio.h"
/* Header UART của ESP-IDF: dùng cho cảm biến bụi SDS011. */
#include "driver/uart.h"
/* Header I2C master driver mới của ESP-IDF v6: dùng cho LCD, TCA, cảm biến I2C, RTC. */
#include "driver/i2c_master.h"

/* Tên thiết bị; được ghi vào file CSV để biết dữ liệu sinh ra từ hệ nào. */
#define DEVICE_NAME "ESP32_5SENSOR_SDS011_IDF_MODULAR_CS5"

/* ========================= Wi-Fi AP ========================= */
/* SSID Wi-Fi do ESP32 tự phát; điện thoại/laptop kết nối vào tên này. */
#define AP_SSID "ESP32-LOGGER"
/* Mật khẩu Wi-Fi AP; tối thiểu 8 ký tự theo yêu cầu WPA/WPA2. */
#define AP_PASS "12345678"

/* ========================= I2C chính ========================= */
/* Dùng I2C port 0 của ESP32. */
#define I2C_PORT        I2C_NUM_0
/* SDA của bus I2C chính nối LCD2004 I2C và TCA9548A. */
#define I2C_SDA         GPIO_NUM_21
/* SCL của bus I2C chính nối LCD2004 I2C và TCA9548A. */
#define I2C_SCL         GPIO_NUM_22
/* Tốc độ I2C 100 kHz: ổn định hơn cho nhiều module + dây dupont. */
#define I2C_FREQ_HZ     100000

/* Địa chỉ I2C của TCA9548A; thường là 0x70 nếu A0/A1/A2 nối GND. */
#define TCA_ADDR        0x70
/* Địa chỉ LCD2004 I2C; scanner của bạn đã thấy 0x27. */
#define LCD_ADDR        0x27
/* Địa chỉ SHT31; module SHT31 thường dùng 0x44. */
#define SHT31_ADDR      0x44
/* Địa chỉ HTU21D; cố định 0x40. */
#define HTU21D_ADDR     0x40
/* AHT10 và AHT20 đều thường dùng 0x38, vì vậy phải tách qua TCA. */
#define AHT_ADDR        0x38
/* DS3231 dùng địa chỉ I2C 0x68. */
#define DS3231_ADDR     0x68

/* ========================= Kênh TCA9548A ========================= */
/* AHT10 đặt ở kênh 0 theo sơ đồ của đề tài. */
#define CH_AHT10        0
/* AHT20 đặt ở kênh 1. */
#define CH_AHT20        1
/* HTU21D đặt ở kênh 2. */
#define CH_HTU21D       2
/* SHT31 đặt ở kênh 3 và được dùng làm cảm biến tham chiếu khi xử lý dữ liệu. */
#define CH_SHT31        3
/* DS3231 đặt ở kênh 4 để lấy thời gian thực cho file CSV. */
#define CH_DS3231       4

/* ========================= DHT11 ========================= */
/* DHT11 dùng giao tiếp 1 dây, DATA nối GPIO4. */
#define DHT_PIN         GPIO_NUM_4

/* ========================= SDS011 UART2 ========================= */
/* Dùng UART2 để không ảnh hưởng UART0 nạp code/monitor. */
#define SDS_UART        UART_NUM_2
/* RX của ESP32 nhận dữ liệu từ TX của SDS011. */
#define SDS_RX          GPIO_NUM_16
/* TX của ESP32 gửi lệnh tới RX của SDS011, hiện chủ yếu để sẵn. */
#define SDS_TX          GPIO_NUM_17
/* SDS011 dùng baudrate 9600 theo giao thức mặc định. */
#define SDS_BAUD        9600

/* ========================= microSD SPI ========================= */
/* Thư mục mount thẻ SD trong ESP-IDF; file CSV nằm trong /sdcard. */
#define SD_MOUNT_POINT  "/sdcard"
/* CS của module SD: BẢN NÀY ĐÃ SỬA THEO MẠCH CỦA BẠN LÀ GPIO5. */
#define SD_CS           GPIO_NUM_5
/* SCK của SPI dùng chân VSPI phổ biến GPIO18. */
#define SD_SCK          GPIO_NUM_18
/* MISO: dữ liệu từ thẻ SD về ESP32, module SD có thể ghi DO. */
#define SD_MISO         GPIO_NUM_19
/* MOSI: dữ liệu từ ESP32 sang thẻ SD, module SD có thể ghi DI. */
#define SD_MOSI         GPIO_NUM_23

/* ========================= Nút nhấn ========================= */
/* Code dùng pull-up nội: một đầu nút vào GPIO, đầu còn lại xuống GND. */
#define BTN_UP          GPIO_NUM_32
#define BTN_DOWN        GPIO_NUM_33
#define BTN_OK          GPIO_NUM_25
#define BTN_BACK        GPIO_NUM_26

/* ========================= Chu kỳ hoạt động ========================= */
/* Chu kỳ đọc cảm biến: 5 giây/lần để DHT11 đỡ lỗi hơn. */
#define SENSOR_PERIOD_MS    5000
/* Chu kỳ refresh LCD: 0.5 giây/lần để hiển thị tương đối tức thì. */
#define LCD_PERIOD_MS       500
/* Chu kỳ lưu mẫu: 5 phút/lần theo yêu cầu đo dài hạn. */
#define LOG_PERIOD_MS       300000
/* Nếu quá 10 giây không nhận frame SDS011 thì coi SDS011 là lỗi/NA. */
#define SDS_TIMEOUT_MS      10000

#endif

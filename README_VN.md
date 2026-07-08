# ESP32 Temperature & Humidity Logger
hi
Project này là firmware ESP-IDF cho hệ thống đo và so sánh nhiều cảm biến nhiệt độ - độ ẩm trong thời gian dài. Hệ thống dùng ESP32 làm bộ điều khiển trung tâm, đọc dữ liệu từ các cảm biến, hiển thị lên LCD, lưu dữ liệu vào thẻ microSD và tạo Web Server nội bộ để xem/tải file CSV.

## Mục tiêu

Hệ thống được xây dựng để phục vụ bài tập lớn môn Kỹ thuật Vi xử lý. Mục tiêu chính là thu thập dữ liệu dài hạn từ nhiều cảm biến môi trường, sau đó so sánh độ lệch và độ ổn định của từng cảm biến trong cùng điều kiện đo.

Các cảm biến nhiệt độ - độ ẩm được sử dụng:

- DHT11
- SHT31
- HTU21D
- AHT10
- AHT20

Ngoài ra, hệ thống có tích hợp thêm SDS011 để ghi dữ liệu bụi mịn PM2.5 và PM10.

## Phần cứng sử dụng

| Thiết bị | Chức năng |
|---|---|
| ESP32 | Vi điều khiển trung tâm |
| TCA9548A | Bộ chia kênh I2C |
| DHT11 | Cảm biến nhiệt độ - độ ẩm, đọc qua GPIO |
| SHT31 | Cảm biến nhiệt độ - độ ẩm, dùng làm tham chiếu tương đối |
| HTU21D | Cảm biến nhiệt độ - độ ẩm |
| AHT10 | Cảm biến nhiệt độ - độ ẩm |
| AHT20 | Cảm biến nhiệt độ - độ ẩm |
| SDS011 | Cảm biến bụi mịn PM2.5/PM10 |
| DS3231 | Module thời gian thực |
| LCD2004 I2C | Hiển thị dữ liệu và trạng thái hệ thống |
| microSD module | Lưu dữ liệu CSV |
| Nút nhấn | Điều khiển menu và START/STOP phiên đo |

## Kết nối chính

Một số chân quan trọng trong firmware:

| Chức năng | Chân ESP32 |
|---|---|
| I2C SDA | GPIO21 |
| I2C SCL | GPIO22 |
| SD CS | GPIO5 |
| SD SCK | GPIO18 |
| SD MISO | GPIO19 |
| SD MOSI | GPIO23 |
| SDS011 RX2 | GPIO16 |
| SDS011 TX2 | GPIO17 |
| DHT11 DATA | GPIO4 |
| UP | GPIO32 |
| DOWN | GPIO33 |
| OK | GPIO25 |
| BACK | GPIO26 |

TCA9548A được dùng để tách các cảm biến I2C ra từng kênh riêng, tránh xung đột địa chỉ và giúp dễ kiểm tra lỗi từng cảm biến.

## Cấu trúc source code

```text
.
├── CMakeLists.txt
├── sdkconfig.defaults
├── README_VN.md
└── main
    ├── CMakeLists.txt
    ├── app_main.c
    ├── app_config.h
    ├── app_state.c
    ├── app_state.h
    ├── app_types.h
    ├── i2c_bus.c
    ├── i2c_bus.h
    ├── lcd2004.c
    ├── lcd2004.h
    ├── rtc_ds3231.c
    ├── rtc_ds3231.h
    ├── sensors.c
    ├── sensors.h
    ├── sds011.c
    ├── sds011.h
    ├── storage_sd.c
    ├── storage_sd.h
    ├── ui_menu.c
    ├── ui_menu.h
    ├── web_server.c
    └── web_server.h
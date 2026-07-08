# ESP32 Logger IDF v6 - Modular - CS SD GPIO5 - Có chú thích học code

Bản này là project ESP-IDF tách file `.h/.c`, được sửa theo mạch của bạn:

- `SD CS = GPIO5`
- `SD SCK = GPIO18`
- `SD MISO = GPIO19`
- `SD MOSI = GPIO23`

Các file được chú thích nhiều nhất để học:

- `main/app_config.h`: nơi khai báo toàn bộ chân, kênh TCA, chu kỳ đọc/lưu.
- `main/app_main.c`: luồng chạy chính kiểu Arduino `.ino`.
- `main/storage_sd.c`: khởi tạo SD, tạo file CSV, ghi dữ liệu, resume session.
- `main/ui_menu.c`: xử lý nút và menu LCD.
- `main/sensors.c`: đọc DHT11, SHT31, HTU21D, AHT10, AHT20.

## Sơ đồ chân SD đúng với bản này

```text
MicroSD CS   -> ESP32 GPIO5
MicroSD SCK  -> ESP32 GPIO18
MicroSD MISO -> ESP32 GPIO19
MicroSD MOSI -> ESP32 GPIO23
MicroSD GND  -> ESP32 GND
MicroSD VCC  -> 5V hoặc 3.3V tùy module
```

Lưu ý: nếu module SD có IC ổn áp/chuyển mức thì thường cấp 5V. Nếu module trần thì cấp 3.3V.

## Build và nạp

Mở ESP-IDF Terminal trong VS Code, chạy:

```bash
idf.py fullclean
idf.py set-target esp32
idf.py build
idf.py -p COM11 flash monitor
```

Nếu SD vẫn ERR, kiểm tra thêm:

1. Thẻ đã format FAT32 chưa.
2. Dây DO/DI có bị đấu nhầm không: DO=MISO, DI=MOSI.
3. Module SD có cần cấp 5V không.
4. Trong `storage_sd.c` hiện đã đặt `host.max_freq_khz = 400` để test ổn định; nếu SD nhận rồi có thể tăng lên 1000 hoặc 4000.

/*
 * storage_sd.c
 * ------------------------------------------------------------
 * File này quản lý 3 việc chính:
 * 1. Khởi tạo thẻ microSD qua SPI.
 * 2. Tạo/ghi file CSV cho từng phiên đo.
 * 3. Lưu trạng thái START/STOP vào NVS để reset vẫn resume file cũ.
 */

#include <stdio.h>          // FILE, fopen, fprintf, fclose
#include <string.h>         // strcmp, snprintf
#include <sys/stat.h>       // stat để kiểm tra file đã tồn tại chưa
#include "esp_log.h"       // ESP_LOGE, esp_err_to_name
#include "esp_err.h"       // esp_err_t
#include "nvs.h"           // NVS lưu trạng thái đo
#include "driver/spi_common.h"   // SPI2_HOST, spi_bus_initialize
#include "driver/sdspi_host.h"   // SDSPI_HOST_DEFAULT, SDSPI_DEVICE_CONFIG_DEFAULT
#include "esp_vfs_fat.h"         // esp_vfs_fat_sdspi_mount
#include "sdmmc_cmd.h"           // sdmmc_card_t, sdmmc_card_print_info

#include "storage_sd.h"
#include "rtc_ds3231.h"
#include "app_config.h"
#include "app_state.h"

/* Kiểm tra một đường dẫn file có tồn tại chưa. */
static bool file_exists(const char *path) {
    struct stat st;                 // biến nhận thông tin file nếu file tồn tại
    return stat(path, &st) == 0;    // stat trả 0 nghĩa là file tồn tại
}

/* Tự tìm tên file mới dạng S0001.CSV, S0002.CSV,... */
static void make_session_filename(char *out, size_t n) {
    for (int i = 1; i <= 9999; i++) {                         // thử từ 1 đến 9999 phiên
        char path[96];                                        // buffer chứa đường dẫn đầy đủ
        snprintf(path, sizeof(path), SD_MOUNT_POINT "/S%04d.CSV", i); // tạo /sdcard/S0001.CSV
        if (!file_exists(path)) {                             // nếu file chưa có thì dùng tên này
            snprintf(out, n, "S%04d.CSV", i);                // lưu tên ngắn vào currentSessionFile
            return;                                           // thoát sau khi tìm được tên hợp lệ
        }
    }
    snprintf(out, n, "S9999.CSV");                            // dự phòng nếu hết số file
}

/* Lưu trạng thái đo vào NVS để khi ESP32 reset vẫn biết trước đó đang đo hay không. */
void save_state(void) {
    nvs_handle_t h;                                           // handle làm việc với namespace NVS
    if (nvs_open("logger", NVS_READWRITE, &h) == ESP_OK) {    // mở namespace logger để ghi
        nvs_set_i8(h, "measuring", measuring ? 1 : 0);        // lưu trạng thái START/STOP
        nvs_set_i8(h, "autolog", autoLog ? 1 : 0);            // lưu có cho phép ghi SD hay không
        nvs_set_i8(h, "lcdlight", lcdLightOn ? 1 : 0);        // lưu trạng thái đèn nền LCD
        nvs_set_str(h, "sessfile", currentSessionFile);       // lưu tên file session hiện tại
        nvs_commit(h);                                        // commit để dữ liệu thật sự ghi vào flash
        nvs_close(h);                                         // đóng NVS sau khi ghi xong
    }
}

/* Đọc trạng thái cũ từ NVS khi ESP32 vừa khởi động. */
void load_state(void) {
    nvs_handle_t h;                                           // handle NVS
    if (nvs_open("logger", NVS_READONLY, &h) == ESP_OK) {     // mở namespace chỉ đọc
        int8_t v = 0;                                         // biến tạm đọc giá trị 0/1
        if (nvs_get_i8(h, "measuring", &v) == ESP_OK) measuring = v != 0; // khôi phục trạng thái đo
        if (nvs_get_i8(h, "autolog", &v) == ESP_OK) autoLog = v != 0;     // khôi phục autoLog
        if (nvs_get_i8(h, "lcdlight", &v) == ESP_OK) lcdLightOn = v != 0; // khôi phục đèn LCD
        size_t len = sizeof(currentSessionFile);              // kích thước buffer tên file
        nvs_get_str(h, "sessfile", currentSessionFile, &len); // đọc tên file nếu có
        nvs_close(h);                                         // đóng NVS
    }
}

/* Tạo file CSV mới khi người dùng bấm START. */
static bool create_session_file(void) {
    if (!sdReady) {                                           // nếu SD chưa sẵn sàng thì không tạo file
        snprintf(currentSessionFile, sizeof(currentSessionFile), "NO_SD"); // báo trạng thái không có SD
        measuring = false;                                    // không cho đo lưu nếu SD lỗi
        save_state();                                         // lưu lại trạng thái này
        return false;                                         // báo tạo file thất bại
    }

    make_session_filename(currentSessionFile, sizeof(currentSessionFile)); // tìm tên file mới

    char path[128];                                           // đường dẫn đầy đủ tới file CSV
    snprintf(path, sizeof(path), SD_MOUNT_POINT "/%s", currentSessionFile); // ghép /sdcard + tên file

    FILE *f = fopen(path, "w");                              // mở file mới ở chế độ ghi mới
    if (!f) {                                                 // nếu fopen thất bại
        snprintf(currentSessionFile, sizeof(currentSessionFile), "ERR"); // báo lỗi file
        return false;                                         // tạo file thất bại
    }

    char dt[32];                                              // buffer thời gian dạng yyyy-mm-dd hh:mm:ss
    get_datetime_str(dt, sizeof(dt));                         // lấy thời gian hiện tại từ DS3231

    fprintf(f, "SESSION_START,%s\n", dt);                    // dòng đánh dấu bắt đầu phiên đo
    fprintf(f, "device,%s\n", DEVICE_NAME);                  // dòng ghi tên thiết bị
    fprintf(f, "rtc_time,dht_t,dht_h,dht_ok,sht_t,sht_h,sht_ok,htu_t,htu_h,htu_ok,aht10_t,aht10_h,aht10_ok,aht20_t,aht20_h,aht20_ok,sds_pm25,sds_pm10,sds_ok\n"); // header CSV
    fclose(f);                                                // đóng file sau khi ghi header

    save_state();                                             // lưu tên file vào NVS
    return true;                                              // tạo file thành công
}

/* Ghi 1 dòng dữ liệu vào file CSV, gọi mỗi 5 phút khi đang đo. */
void append_session_row(void) {
    if (!sdReady || !measuring || !autoLog) return;           // không ghi nếu SD lỗi/chưa START/tắt log
    if (!strcmp(currentSessionFile, "NONE") || !strcmp(currentSessionFile, "NO_SD") || !strcmp(currentSessionFile, "ERR")) return; // bỏ qua tên file không hợp lệ

    sensor_data_t dht, sht, htu, a10, a20;                    // biến copy dữ liệu cảm biến nhiệt ẩm
    sds_data_t sds;                                           // biến copy dữ liệu bụi SDS011

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) { // khóa mutex để copy dữ liệu an toàn
        dht = dhtData;                                        // copy DHT11
        sht = shtData;                                        // copy SHT31
        htu = htuData;                                        // copy HTU21D
        a10 = aht10Data;                                      // copy AHT10
        a20 = aht20Data;                                      // copy AHT20
        sds = sdsData;                                        // copy SDS011
        xSemaphoreGive(dataMutex);                            // mở khóa dữ liệu
    } else {
        return;                                               // nếu không lấy được mutex thì bỏ lần ghi này
    }

    char path[128];                                           // đường dẫn file session hiện tại
    snprintf(path, sizeof(path), SD_MOUNT_POINT "/%s", currentSessionFile); // tạo /sdcard/Sxxxx.CSV

    FILE *f = fopen(path, "a");                              // mở file ở chế độ append để ghi nối tiếp
    if (!f) return;                                           // nếu mở thất bại thì bỏ qua

    char dt[32];                                              // buffer thời gian
    get_datetime_str(dt, sizeof(dt));                         // lấy thời gian thực từ DS3231

    char dht_t[16], dht_h[16], sht_t[16], sht_h[16];           // buffer text cho DHT/SHT
    char htu_t[16], htu_h[16], a10_t[16], a10_h[16];           // buffer text cho HTU/AHT10
    char a20_t[16], a20_h[16], pm25[16], pm10[16];             // buffer text cho AHT20/SDS011

    csv_float(dht_t, sizeof(dht_t), dht.t, dht.ok, 2);         // nếu DHT lỗi thì chuỗi rỗng
    csv_float(dht_h, sizeof(dht_h), dht.h, dht.ok, 2);         // độ ẩm DHT
    csv_float(sht_t, sizeof(sht_t), sht.t, sht.ok, 2);         // nhiệt độ SHT31
    csv_float(sht_h, sizeof(sht_h), sht.h, sht.ok, 2);         // độ ẩm SHT31
    csv_float(htu_t, sizeof(htu_t), htu.t, htu.ok, 2);         // nhiệt độ HTU21D
    csv_float(htu_h, sizeof(htu_h), htu.h, htu.ok, 2);         // độ ẩm HTU21D
    csv_float(a10_t, sizeof(a10_t), a10.t, a10.ok, 2);         // nhiệt độ AHT10
    csv_float(a10_h, sizeof(a10_h), a10.h, a10.ok, 2);         // độ ẩm AHT10
    csv_float(a20_t, sizeof(a20_t), a20.t, a20.ok, 2);         // nhiệt độ AHT20
    csv_float(a20_h, sizeof(a20_h), a20.h, a20.ok, 2);         // độ ẩm AHT20
    csv_float(pm25, sizeof(pm25), sds.pm25, sds.ok, 1);        // PM2.5 SDS011
    csv_float(pm10, sizeof(pm10), sds.pm10, sds.ok, 1);        // PM10 SDS011

    fprintf(f, "%s,%s,%s,%d,%s,%s,%d,%s,%s,%d,%s,%s,%d,%s,%s,%d,%s,%s,%d\n", // ghi 1 dòng CSV
            dt,                                                // thời gian đo
            dht_t, dht_h, dht.ok ? 1 : 0,                      // DHT11: T,H,OK
            sht_t, sht_h, sht.ok ? 1 : 0,                      // SHT31: T,H,OK
            htu_t, htu_h, htu.ok ? 1 : 0,                      // HTU21D: T,H,OK
            a10_t, a10_h, a10.ok ? 1 : 0,                      // AHT10: T,H,OK
            a20_t, a20_h, a20.ok ? 1 : 0,                      // AHT20: T,H,OK
            pm25, pm10, sds.ok ? 1 : 0);                       // SDS011: PM2.5, PM10, OK

    fclose(f);                                                // đóng file ngay để tránh mất dữ liệu khi mất điện
    savedRows++;                                              // tăng bộ đếm số mẫu đã lưu trong RAM
}

/* Hàm xử lý khi người dùng bấm START. */
void start_measurement(void) {
    if (measuring) return;                                    // nếu đang đo rồi thì không làm gì
    if (!create_session_file()) return;                       // tạo file mới, nếu lỗi thì không START
    measuring = true;                                        // bật trạng thái đang đo
    save_state();                                             // lưu trạng thái vào NVS để reset vẫn nhớ
}

/* Hàm xử lý khi người dùng bấm STOP. */
void stop_measurement(void) {
    if (sdReady && measuring && strcmp(currentSessionFile, "NONE") && strcmp(currentSessionFile, "NO_SD") && strcmp(currentSessionFile, "ERR")) { // chỉ ghi STOP nếu file hợp lệ
        char path[128];                                       // đường dẫn file session
        snprintf(path, sizeof(path), SD_MOUNT_POINT "/%s", currentSessionFile); // tạo path
        FILE *f = fopen(path, "a");                          // mở file để ghi dòng STOP
        if (f) {                                             // nếu mở được file
            char dt[32];                                     // buffer thời gian
            get_datetime_str(dt, sizeof(dt));                // lấy thời gian hiện tại
            fprintf(f, "SESSION_STOP,%s\n", dt);            // ghi đánh dấu kết thúc phiên
            fclose(f);                                       // đóng file
        }
    }

    measuring = false;                                       // tắt trạng thái đo
    snprintf(currentSessionFile, sizeof(currentSessionFile), "NONE"); // xóa tên file hiện tại
    save_state();                                            // lưu trạng thái mới
}

/* Khi reset giữa chừng, hàm này nối tiếp file cũ thay vì tạo file mới. */
void resume_session_if_needed(void) {
    load_state();                                             // đọc trạng thái cũ từ NVS

    if (!measuring) {                                         // nếu trước khi reset không đo
        snprintf(currentSessionFile, sizeof(currentSessionFile), "NONE"); // không có file hiện tại
        return;                                               // thoát
    }

    if (!sdReady) {                                           // nếu trước đó đang đo nhưng SD hiện lỗi
        snprintf(currentSessionFile, sizeof(currentSessionFile), "NO_SD"); // báo lỗi SD
        return;                                               // giữ measuring để biết trạng thái nhưng chưa ghi được
    }

    char path[128];                                           // đường dẫn file cũ
    snprintf(path, sizeof(path), SD_MOUNT_POINT "/%s", currentSessionFile); // ghép đường dẫn

    if (!file_exists(path)) {                                 // nếu file cũ không còn trên SD
        measuring = false;                                    // hủy trạng thái đo
        snprintf(currentSessionFile, sizeof(currentSessionFile), "NONE");  // xóa file hiện tại
        save_state();                                         // lưu lại
        return;                                               // thoát
    }

    FILE *f = fopen(path, "a");                              // mở file cũ để ghi dòng resume
    if (f) {                                                  // nếu mở thành công
        char dt[32];                                          // buffer thời gian
        get_datetime_str(dt, sizeof(dt));                     // lấy thời gian hiện tại
        fprintf(f, "SESSION_RESUME,%s\n", dt);               // ghi đánh dấu hệ thống resume sau reset
        fclose(f);                                            // đóng file
    }
}

/* Khởi tạo microSD qua SPI. Đây là phần quan trọng nhất khi SD bị ERR. */
void init_sd(void) {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {          // cấu hình mount FATFS
        .format_if_mount_failed = false,                       // không tự format thẻ nếu mount lỗi
        .max_files = 5,                                        // số file tối đa mở cùng lúc
        .allocation_unit_size = 16 * 1024                      // block cấp phát FATFS
    };

    sdmmc_card_t *card = NULL;                                 // con trỏ nhận thông tin thẻ sau khi mount
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();                  // tạo cấu hình host SDSPI mặc định
    host.slot = SPI2_HOST;                                     // dùng SPI2_HOST của ESP32
    host.max_freq_khz = 400;                                   // giảm tốc SPI 400 kHz để test ổn định với dây dupont

    spi_bus_config_t bus_cfg = {                               // cấu hình bus SPI vật lý
        .mosi_io_num = SD_MOSI,                                // MOSI lấy từ app_config.h, hiện là GPIO23
        .miso_io_num = SD_MISO,                                // MISO lấy từ app_config.h, hiện là GPIO19
        .sclk_io_num = SD_SCK,                                 // SCK lấy từ app_config.h, hiện là GPIO18
        .quadwp_io_num = -1,                                   // không dùng Quad SPI WP
        .quadhd_io_num = -1,                                   // không dùng Quad SPI HD
        .max_transfer_sz = 4000,                               // kích thước truyền tối đa
    };

    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA); // khởi tạo bus SPI
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {        // INVALID_STATE nghĩa là bus đã init rồi, có thể bỏ qua
        ESP_LOGE(TAG, "SPI init fail: %s", esp_err_to_name(ret)); // in lỗi SPI
        sdReady = false;                                       // đánh dấu SD lỗi
        return;                                                // thoát init SD
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT(); // cấu hình thiết bị SD trên SPI
    slot_config.gpio_cs = SD_CS;                                // CS dùng macro SD_CS = GPIO_NUM_5
    slot_config.host_id = host.slot;                            // gắn thiết bị SD vào SPI2_HOST

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card); // mount SD vào /sdcard
    if (ret != ESP_OK) {                                        // nếu mount thất bại
        ESP_LOGE(TAG, "SD mount fail: %s", esp_err_to_name(ret)); // in lỗi thật: ESP_FAIL, INVALID_STATE,...
        sdReady = false;                                       // báo SD chưa sẵn sàng
        return;                                                // thoát
    }

    sdReady = true;                                            // mount thành công
    sdmmc_card_print_info(stdout, card);                        // in thông tin thẻ ra monitor
}

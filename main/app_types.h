#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdbool.h>
#include <stdint.h>

// Dữ liệu nhiệt độ - độ ẩm
// ok = false nghĩa là mẫu đọc lỗi/NA.
typedef struct {
    float t;
    float h;
    bool ok;
} sensor_data_t;

// Dữ liệu SDS011
typedef struct {
    float pm25;
    float pm10;
    bool ok;
    int64_t last_frame_us;
} sds_data_t;

// Thời gian DS3231
typedef struct {
    int year, mon, day, hour, min, sec;
} rtc_time_t;

typedef enum {
    PAGE_HOME = 0,
    PAGE_SENSOR_1,
    PAGE_SENSOR_2,
    PAGE_DUST,
    PAGE_STATUS,
    PAGE_CONTROL,
    PAGE_MAX
} ui_page_t;

#endif

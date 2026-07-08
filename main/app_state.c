#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_timer.h"
#include "app_state.h"

const char *TAG = "LOGGER_IDF";

sensor_data_t dhtData, shtData, htuData, aht10Data, aht20Data;
sds_data_t sdsData;

bool sdReady = false;
bool rtcReady = false;
bool shtInitOK = false;
bool htuInitOK = false;
bool aht10InitOK = false;
bool aht20InitOK = false;

bool measuring = false;
bool autoLog = true;
bool lcdLightOn = true;
uint32_t savedRows = 0;
char currentSessionFile[32] = "NONE";

ui_page_t currentPage = PAGE_HOME;
int controlIndex = 0;
volatile bool uiDirty = true;
ui_page_t lastDrawnPage = (ui_page_t)-1;

SemaphoreHandle_t dataMutex = NULL;
portMUX_TYPE dhtMux = portMUX_INITIALIZER_UNLOCKED;

sensor_data_t invalid_sensor(void) {
    sensor_data_t d = { NAN, NAN, false };
    return d;
}

const char *on_off(bool v) {
    return v ? "ON" : "OFF";
}

void ftoa_or_na(char *out, size_t n, float v, bool ok, int prec) {
    if (!ok || isnan(v)) snprintf(out, n, "NA");
    else snprintf(out, n, "%.*f", prec, v);
}

void csv_float(char *out, size_t n, float v, bool ok, int prec) {
    if (!ok || isnan(v)) out[0] = 0;
    else snprintf(out, n, "%.*f", prec, v);
}

void json_float(char *out, size_t n, float v, bool ok, int prec) {
    if (!ok || isnan(v)) snprintf(out, n, "null");
    else snprintf(out, n, "%.*f", prec, v);
}

int64_t now_us(void) {
    return esp_timer_get_time();
}

bool elapsed_ms(int64_t start_us, int ms) {
    return (now_us() - start_us) >= ((int64_t)ms * 1000);
}

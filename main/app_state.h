#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "app_types.h"
#include "app_config.h"

extern const char *TAG;

extern sensor_data_t dhtData, shtData, htuData, aht10Data, aht20Data;
extern sds_data_t sdsData;

extern bool sdReady;
extern bool rtcReady;
extern bool shtInitOK;
extern bool htuInitOK;
extern bool aht10InitOK;
extern bool aht20InitOK;

extern bool measuring;
extern bool autoLog;
extern bool lcdLightOn;
extern uint32_t savedRows;
extern char currentSessionFile[32];

extern ui_page_t currentPage;
extern int controlIndex;
extern volatile bool uiDirty;
extern ui_page_t lastDrawnPage;

extern SemaphoreHandle_t dataMutex;
extern portMUX_TYPE dhtMux;

sensor_data_t invalid_sensor(void);
const char *on_off(bool v);
void ftoa_or_na(char *out, size_t n, float v, bool ok, int prec);
void csv_float(char *out, size_t n, float v, bool ok, int prec);
void json_float(char *out, size_t n, float v, bool ok, int prec);
int64_t now_us(void);
bool elapsed_ms(int64_t start_us, int ms);

#endif

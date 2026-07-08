/*
 * main/web_server.c
 * ------------------------------------------------------------
 * File này tạo Wi-Fi AP và HTTP server nội bộ để xem dữ liệu, tải CSV, START/STOP từ trình duyệt.
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "web_server.h"
#include "storage_sd.h"
#include "rtc_ds3231.h"
#include "app_config.h"
#include "app_state.h"

static httpd_handle_t httpServer = NULL;

static esp_err_t send_header(httpd_req_t *req, const char *title) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><head><meta charset='utf-8'>");
    httpd_resp_sendstr_chunk(req, "<meta name='viewport' content='width=device-width,initial-scale=1'>");
    httpd_resp_sendstr_chunk(req, "<title>"); httpd_resp_sendstr_chunk(req, title); httpd_resp_sendstr_chunk(req, "</title>");
    httpd_resp_sendstr_chunk(req, "<style>");
    httpd_resp_sendstr_chunk(req, "body{font-family:Arial;margin:16px;background:#f5f5f5}");
    httpd_resp_sendstr_chunk(req, ".card{background:white;padding:14px;border-radius:12px;margin:12px 0;box-shadow:0 2px 8px #ddd}");
    httpd_resp_sendstr_chunk(req, ".button{display:inline-block;background:#1f6feb;color:white;text-decoration:none;padding:9px 13px;border-radius:8px;margin:4px}");
    httpd_resp_sendstr_chunk(req, "table{border-collapse:collapse;width:100%}td,th{border:1px solid #ddd;padding:7px}code{background:#eee;padding:2px 5px}");
    httpd_resp_sendstr_chunk(req, "</style></head><body>");
    httpd_resp_sendstr_chunk(req, "<div class='card'><a class='button' href='/'>Home</a><a class='button' href='/files'>Files</a><a class='button' href='/data'>JSON</a><a class='button' href='/start'>START</a><a class='button' href='/stop'>STOP</a></div>");
    return ESP_OK;
}

static esp_err_t root_handler(httpd_req_t *req) {
    sensor_data_t dht, sht, htu, a10, a20;
    sds_data_t sds;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        dht = dhtData; sht = shtData; htu = htuData; a10 = aht10Data; a20 = aht20Data; sds = sdsData;
        xSemaphoreGive(dataMutex);
    } else return ESP_FAIL;

    send_header(req, "ESP32 Logger");
    char dt[32]; get_datetime_str(dt, sizeof(dt));
    char buf[512], v1[16], v2[16];
    snprintf(buf, sizeof(buf), "<div class='card'><h2>ESP32 SD Logger</h2><p><b>Time:</b> %s</p><p><b>Measuring:</b> %s</p><p><b>File:</b> %s</p><p><b>Saved:</b> %lu</p></div>", dt, on_off(measuring), currentSessionFile, (unsigned long)savedRows);
    httpd_resp_sendstr_chunk(req, buf);

    httpd_resp_sendstr_chunk(req, "<div class='card'><h3>Temp/Humidity</h3><table><tr><th>Sensor</th><th>T</th><th>H</th><th>OK</th></tr>");
#define ROW_SENSOR(name, data) do { \
    ftoa_or_na(v1, sizeof(v1), (data).t, (data).ok, 1); \
    ftoa_or_na(v2, sizeof(v2), (data).h, (data).ok, 1); \
    snprintf(buf, sizeof(buf), "<tr><td>%s</td><td>%s</td><td>%s</td><td>%d</td></tr>", name, v1, v2, (data).ok ? 1 : 0); \
    httpd_resp_sendstr_chunk(req, buf); \
} while (0)
    ROW_SENSOR("DHT11", dht); ROW_SENSOR("SHT31", sht); ROW_SENSOR("HTU21D", htu); ROW_SENSOR("AHT10", a10); ROW_SENSOR("AHT20", a20);
#undef ROW_SENSOR
    httpd_resp_sendstr_chunk(req, "</table></div>");

    ftoa_or_na(v1, sizeof(v1), sds.pm25, sds.ok, 1);
    ftoa_or_na(v2, sizeof(v2), sds.pm10, sds.ok, 1);
    snprintf(buf, sizeof(buf), "<div class='card'><h3>SDS011</h3><p>PM2.5: %s ug/m3</p><p>PM10: %s ug/m3</p><p>OK: %d</p></div>", v1, v2, sds.ok ? 1 : 0);
    httpd_resp_sendstr_chunk(req, buf);
    httpd_resp_sendstr_chunk(req, "</body></html>");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t data_handler(httpd_req_t *req) {
    sensor_data_t dht, sht, htu, a10, a20;
    sds_data_t sds;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        dht = dhtData; sht = shtData; htu = htuData; a10 = aht10Data; a20 = aht20Data; sds = sdsData;
        xSemaphoreGive(dataMutex);
    } else return ESP_FAIL;

    char dt[32]; get_datetime_str(dt, sizeof(dt));
    char dht_t[16], dht_h[16], sht_t[16], sht_h[16], htu_t[16], htu_h[16], a10_t[16], a10_h[16], a20_t[16], a20_h[16], pm25[16], pm10[16];
    json_float(dht_t, sizeof(dht_t), dht.t, dht.ok, 2); json_float(dht_h, sizeof(dht_h), dht.h, dht.ok, 2);
    json_float(sht_t, sizeof(sht_t), sht.t, sht.ok, 2); json_float(sht_h, sizeof(sht_h), sht.h, sht.ok, 2);
    json_float(htu_t, sizeof(htu_t), htu.t, htu.ok, 2); json_float(htu_h, sizeof(htu_h), htu.h, htu.ok, 2);
    json_float(a10_t, sizeof(a10_t), a10.t, a10.ok, 2); json_float(a10_h, sizeof(a10_h), a10.h, a10.ok, 2);
    json_float(a20_t, sizeof(a20_t), a20.t, a20.ok, 2); json_float(a20_h, sizeof(a20_h), a20.h, a20.ok, 2);
    json_float(pm25, sizeof(pm25), sds.pm25, sds.ok, 1); json_float(pm10, sizeof(pm10), sds.pm10, sds.ok, 1);

    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{\"time\":\"%s\",\"measuring\":%s,\"file\":\"%s\"," 
        "\"dht_t\":%s,\"dht_h\":%s,\"dht_ok\":%d," 
        "\"sht_t\":%s,\"sht_h\":%s,\"sht_ok\":%d," 
        "\"htu_t\":%s,\"htu_h\":%s,\"htu_ok\":%d," 
        "\"aht10_t\":%s,\"aht10_h\":%s,\"aht10_ok\":%d," 
        "\"aht20_t\":%s,\"aht20_h\":%s,\"aht20_ok\":%d," 
        "\"sds_pm25\":%s,\"sds_pm10\":%s,\"sds_ok\":%d}",
        dt, measuring?"true":"false", currentSessionFile,
        dht_t, dht_h, dht.ok?1:0, sht_t, sht_h, sht.ok?1:0, htu_t, htu_h, htu.ok?1:0,
        a10_t, a10_h, a10.ok?1:0, a20_t, a20_h, a20.ok?1:0, pm25, pm10, sds.ok?1:0);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static esp_err_t files_handler(httpd_req_t *req) {
    send_header(req, "Files");
    httpd_resp_sendstr_chunk(req, "<div class='card'><h2>Files on SD</h2><table><tr><th>Name</th><th>Size</th><th>Download</th></tr>");
    DIR *dir = opendir(SD_MOUNT_POINT);
    if (dir) {
        struct dirent *e;
        char buf[768];
        while ((e = readdir(dir)) != NULL) {
            char path[320]; struct stat st;
            snprintf(path, sizeof(path), SD_MOUNT_POINT "/%s", e->d_name);
            if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                snprintf(buf, sizeof(buf), "<tr><td>%s</td><td>%ld B</td><td><a class='button' href='/download?name=%s'>Download</a></td></tr>", e->d_name, (long)st.st_size, e->d_name);
                httpd_resp_sendstr_chunk(req, buf);
            }
        }
        closedir(dir);
    }
    httpd_resp_sendstr_chunk(req, "</table></div></body></html>");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static bool safe_name(const char *name) {
    return name && name[0] && strstr(name, "..") == NULL && strchr(name, '/') == NULL;
}

static esp_err_t download_handler(httpd_req_t *req) {
    char query[320], name[256];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK || httpd_query_key_value(query, "name", name, sizeof(name)) != ESP_OK || !safe_name(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad name"); return ESP_OK;
    }
    char path[320]; snprintf(path, sizeof(path), SD_MOUNT_POINT "/%s", name);
    FILE *f = fopen(path, "r");
    if (!f) { httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found"); return ESP_OK; }
    httpd_resp_set_type(req, "text/csv");
    char disp[320]; snprintf(disp, sizeof(disp), "attachment; filename=\"%s\"", name);
    httpd_resp_set_hdr(req, "Content-Disposition", disp);
    char buf[512]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) httpd_resp_send_chunk(req, buf, n);
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t start_handler(httpd_req_t *req) {
    start_measurement();
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "Started");
    return ESP_OK;
}

static esp_err_t stop_handler(httpd_req_t *req) {
    stop_measurement();
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "Stopped");
    return ESP_OK;
}

void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    if (httpd_start(&httpServer, &config) != ESP_OK) return;
    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_handler };
    httpd_uri_t data = { .uri = "/data", .method = HTTP_GET, .handler = data_handler };
    httpd_uri_t files = { .uri = "/files", .method = HTTP_GET, .handler = files_handler };
    httpd_uri_t dl = { .uri = "/download", .method = HTTP_GET, .handler = download_handler };
    httpd_uri_t start = { .uri = "/start", .method = HTTP_GET, .handler = start_handler };
    httpd_uri_t stop = { .uri = "/stop", .method = HTTP_GET, .handler = stop_handler };
    httpd_register_uri_handler(httpServer, &root);
    httpd_register_uri_handler(httpServer, &data);
    httpd_register_uri_handler(httpServer, &files);
    httpd_register_uri_handler(httpServer, &dl);
    httpd_register_uri_handler(httpServer, &start);
    httpd_register_uri_handler(httpServer, &stop);
}

void wifi_init_ap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {0};
    strcpy((char *)wifi_config.ap.ssid, AP_SSID);
    strcpy((char *)wifi_config.ap.password, AP_PASS);
    wifi_config.ap.ssid_len = strlen(AP_SSID);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    if (strlen(AP_PASS) == 0) wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi AP started. SSID=%s PASS=%s IP=192.168.4.1", AP_SSID, AP_PASS);
}

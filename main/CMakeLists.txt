idf_component_register(
    SRCS
        "app_main.c"
        "app_state.c"
        "i2c_bus.c"
        "lcd2004.c"
        "rtc_ds3231.c"
        "sensors.c"
        "sds011.c"
        "storage_sd.c"
        "ui_menu.c"
        "web_server.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_uart
        esp_driver_spi
        esp_driver_sdspi
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        nvs_flash
        fatfs
        sdmmc
        esp_timer
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=format-truncation
    -Wno-format-truncation
)

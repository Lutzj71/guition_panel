idf_component_register(
    SRCS "main.c" "board.c" "grbl_link.c" "settings.c" "gcode_stream.c" "sdcard.c"
         "wifi_mgr.c" "http_upload.c"
         "ui.c" "ui_common.c" "ui_main.c" "ui_jog.c" "ui_run.c" "ui_alarm.c"
         "ui_settings.c" "ui_files.c" "ui_wifi.c"
    INCLUDE_DIRS "."
    EMBED_FILES "../fonts/Montserrat-Medium.ttf"
    PRIV_REQUIRES esp_lcd esp_hw_support esp_driver_gpio esp_driver_i2c esp_driver_uart esp_timer
                  freertos fatfs sdmmc esp_driver_sdmmc sd_pwr_ctrl
                  esp_wifi esp_netif esp_event esp_http_server nvs_flash)

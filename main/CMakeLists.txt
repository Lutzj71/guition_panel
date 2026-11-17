idf_component_register(
    SRCS "main.c" "board.c" "grbl_link.c" "ui.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_lcd esp_hw_support esp_driver_gpio esp_driver_i2c esp_driver_uart freertos)

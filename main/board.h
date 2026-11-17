#pragma once

// Guition JC4880P443C_I_W - stałe płyty. Źródła: opisy społecznościowe
// (github.com/ultramcu/guition-jc4880p443c-i-w, github.com/seobaeksol/JC4880P443C_I_W);
// do potwierdzenia na sprzęcie.

#include "driver/gpio.h"
#include "driver/uart.h"
#include "lvgl.h"

#define LCD_H_RES            480
#define LCD_V_RES            800
#define LCD_RST_GPIO         GPIO_NUM_5
#define LCD_BACKLIGHT_GPIO   GPIO_NUM_23      // zwykły GPIO, aktywny stanem wysokim (nie LEDC)
#define LCD_DSI_LANES        2
#define LCD_DSI_MBPS         500
#define LCD_DPHY_LDO_CHAN    3
#define LCD_DPHY_LDO_MV      2500
#define LCD_DPI_CLK_MHZ      34

#define I2C_SDA_GPIO         GPIO_NUM_7       // dotyk GT911 (adres 0x5D), współdzielona z kodekiem audio
#define I2C_SCL_GPIO         GPIO_NUM_8
#define TOUCH_RST_GPIO       GPIO_NUM_22      // wg schematu (dokumentacja dla Arduino podaje -1)
#define TOUCH_INT_GPIO       GPIO_NUM_21

// Łącze z grblHAL (złącze CN2: 1 VIN 5 V, 2 TX0, 3 RX0, 4 GND)
// Uwaga: TX0 panelu -> RX płyty Creality (PB11), RX0 panelu <- TX płyty (PB10).
#define GRBL_UART_NUM        UART_NUM_0
#define GRBL_UART_TX_GPIO    GPIO_NUM_37
#define GRBL_UART_RX_GPIO    GPIO_NUM_38
#define GRBL_UART_BAUD       115200

// Inicjalizuje LDO, podświetlenie, DSI + ST7701, dotyk GT911 i LVGL.
// Zwraca wyświetlacz LVGL albo NULL przy błędzie.
lv_disp_t *board_init(void);

#include "grbl_link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "grbl";

#define POLL_MS   200
#define STALE_US  1500000

static grbl_status_t s_st;
static int64_t s_last_report_us;
static SemaphoreHandle_t s_mtx;

// <Idle|MPos:0.000,0.000,0.000|FS:0,0|...>
static void parse_status(char *line)
{
    char *end = strchr(line, '>');
    if (!end) return;
    *end = 0;
    line++;  // za '<'

    grbl_status_t n;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    n = s_st;
    xSemaphoreGive(s_mtx);

    char *save = NULL;
    char *tok = strtok_r(line, "|", &save);
    if (tok) { strncpy(n.state, tok, sizeof n.state - 1); n.state[sizeof n.state - 1] = 0; }
    while ((tok = strtok_r(NULL, "|", &save))) {
        if (!strncmp(tok, "MPos:", 5) || !strncmp(tok, "WPos:", 5)) {
            n.wpos = tok[0] == 'W';
            n.x = strtof(tok + 5, &tok);
            if (*tok == ',') n.y = strtof(tok + 1, NULL);
        } else if (!strncmp(tok, "FS:", 3)) {
            n.feed = strtof(tok + 3, &tok);
            if (*tok == ',') n.spindle = strtof(tok + 1, NULL);
        } else if (!strncmp(tok, "F:", 2)) {
            n.feed = strtof(tok + 2, NULL);
        }
    }
    n.valid = true;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_st = n;
    s_last_report_us = esp_timer_get_time();
    xSemaphoreGive(s_mtx);
}

static void handle_line(char *line)
{
    if (line[0] == '<') {
        parse_status(line);
    } else if (!strncmp(line, "ok", 2) || !strncmp(line, "error:", 6) || !strncmp(line, "ALARM:", 6)) {
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        strncpy(s_st.last_msg, line, sizeof s_st.last_msg - 1);
        xSemaphoreGive(s_mtx);
    }
}

static void rx_task(void *arg)
{
    char line[200];
    size_t n = 0;
    uint8_t b;
    for (;;) {
        if (uart_read_bytes(GRBL_UART_NUM, &b, 1, pdMS_TO_TICKS(50)) != 1) continue;
        if (b == '\r') continue;
        if (b == '\n') {
            line[n] = 0;
            if (n) handle_line(line);
            n = 0;
        } else if (n < sizeof line - 1) {
            line[n++] = (char)b;
        } else {
            n = 0;  // zbyt długa linia - odrzuć
        }
    }
}

static void poll_task(void *arg)
{
    for (;;) {
        grbl_send_realtime('?');
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

void grbl_link_init(void)
{
    s_mtx = xSemaphoreCreateMutex();

    const uart_config_t cfg = {
        .baud_rate = GRBL_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(GRBL_UART_NUM, 1024, 512, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(GRBL_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(GRBL_UART_NUM, GRBL_UART_TX_GPIO, GRBL_UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(rx_task, "grbl_rx", 4096, NULL, 5, NULL);
    xTaskCreate(poll_task, "grbl_poll", 2048, NULL, 4, NULL);

    // Przejęcie sterowania przez MPG: grblHAL przełącza się znakiem 0x8B.
    // Wysyłamy go tylko, gdy stan jest nieznany - ponowne wysłanie odwróciłoby przełączenie.
    ESP_LOGI(TAG, "łącze grblHAL: UART%d %d baud, TX=%d RX=%d", GRBL_UART_NUM, GRBL_UART_BAUD,
             GRBL_UART_TX_GPIO, GRBL_UART_RX_GPIO);
}

void grbl_link_get_status(grbl_status_t *out)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    *out = s_st;
    out->stale = !s_st.valid || (esp_timer_get_time() - s_last_report_us) > STALE_US;
    xSemaphoreGive(s_mtx);
}

bool grbl_send_line(const char *line)
{
    size_t n = strlen(line);
    if (uart_write_bytes(GRBL_UART_NUM, line, n) < 0) return false;
    return uart_write_bytes(GRBL_UART_NUM, "\n", 1) == 1;
}

void grbl_send_realtime(char c)
{
    uart_write_bytes(GRBL_UART_NUM, &c, 1);
}

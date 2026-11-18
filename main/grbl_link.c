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

#define POLL_MS     200
#define STALE_US    1500000
#define MAX_CB      4
#define AUTO_MPG_MS 2500    // po tylu ms bez raportu panel raz przejmuje sterowanie (0x8B)

static grbl_status_t s_st = {.mpg = -1, .ov_feed = 100, .ov_rapid = 100, .ov_spindle = 100};
static float s_wco_x, s_wco_y;
static bool s_wco_known;
static int64_t s_last_report_us;
static SemaphoreHandle_t s_mtx, s_tx_mtx;

static grbl_resp_cb_t s_resp_cb[MAX_CB]; static void *s_resp_ctx[MAX_CB]; static int s_resp_n;
static grbl_line_cb_t s_line_cb[MAX_CB]; static void *s_line_ctx[MAX_CB]; static int s_line_n;

static grbl_state_t classify(const char *s)
{
    static const struct { const char *p; grbl_state_t id; } map[] = {
        {"Idle", GRBL_ST_IDLE}, {"Run", GRBL_ST_RUN}, {"Hold", GRBL_ST_HOLD}, {"Jog", GRBL_ST_JOG},
        {"Alarm", GRBL_ST_ALARM}, {"Home", GRBL_ST_HOME}, {"Door", GRBL_ST_DOOR},
        {"Check", GRBL_ST_CHECK}, {"Sleep", GRBL_ST_SLEEP},
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
        if (!strncmp(s, map[i].p, strlen(map[i].p))) return map[i].id;
    return GRBL_ST_UNKNOWN;
}

// <Idle|MPos:0.000,0.000,0.000|FS:0,0|WCO:0.000,0.000,0.000|Ov:100,100,100|A:S|MPG:1>
static void parse_status(char *line)
{
    char *end = strchr(line, '>');
    if (!end) return;
    *end = 0;
    line++;  // za '<'

    static bool first = true;
    if (first) { first = false; ESP_LOGI(TAG, "pierwszy raport: <%s>", line); }

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    grbl_status_t n = s_st;
    xSemaphoreGive(s_mtx);
    grbl_status_t prev = n;

    float px = n.mx, py = n.my;
    bool is_wpos = false, have_pos = false;
    n.laser_on = false;   // pole A: jest w raporcie tylko gdy któryś akcesoriów jest aktywny

    char *save = NULL;
    char *tok = strtok_r(line, "|", &save);
    if (tok) {
        strncpy(n.state, tok, sizeof n.state - 1);
        n.state[sizeof n.state - 1] = 0;
        n.state_id = classify(n.state);
    }
    while ((tok = strtok_r(NULL, "|", &save))) {
        char *p;
        if (!strncmp(tok, "MPos:", 5) || !strncmp(tok, "WPos:", 5)) {
            is_wpos = tok[0] == 'W';
            px = strtof(tok + 5, &p);
            if (*p == ',') py = strtof(p + 1, NULL);
            have_pos = true;
        } else if (!strncmp(tok, "WCO:", 4)) {
            s_wco_x = strtof(tok + 4, &p);
            if (*p == ',') s_wco_y = strtof(p + 1, NULL);
            s_wco_known = true;
        } else if (!strncmp(tok, "FS:", 3)) {
            n.feed = strtof(tok + 3, &p);
            if (*p == ',') n.spindle = strtof(p + 1, NULL);
        } else if (!strncmp(tok, "F:", 2)) {
            n.feed = strtof(tok + 2, NULL);
        } else if (!strncmp(tok, "Ov:", 3)) {
            n.ov_feed = (int)strtol(tok + 3, &p, 10);
            if (*p == ',') n.ov_rapid = (int)strtol(p + 1, &p, 10);
            if (*p == ',') n.ov_spindle = (int)strtol(p + 1, NULL, 10);
        } else if (!strncmp(tok, "A:", 2)) {
            n.laser_on = strchr(tok + 2, 'S') || strchr(tok + 2, 'C');
        } else if (!strncmp(tok, "MPG:", 4)) {
            n.mpg = atoi(tok + 4);
        }
    }
    if (have_pos) {
        float ox = s_wco_known ? s_wco_x : 0, oy = s_wco_known ? s_wco_y : 0;
        if (is_wpos) { n.wx = px; n.wy = py; n.mx = px + ox; n.my = py + oy; }
        else         { n.mx = px; n.my = py; n.wx = px - ox; n.wy = py - oy; }
    }
    if (n.state_id != GRBL_ST_ALARM) n.alarm = 0;
    n.valid = true;

    if (!prev.valid || strcmp(prev.state, n.state)) ESP_LOGI(TAG, "stan: %s", n.state);
    if (prev.mpg != n.mpg) ESP_LOGI(TAG, "MPG: %d (%s)", n.mpg, n.mpg == 1 ? "steruje panel" : n.mpg == 0 ? "steruje komputer" : "?");

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_st = n;
    s_last_report_us = esp_timer_get_time();
    xSemaphoreGive(s_mtx);
}

static void handle_line(char *line)
{
    if (line[0] == '<') {
        parse_status(line);
        return;
    }
    bool is_ok = !strcmp(line, "ok");
    bool is_err = !strncmp(line, "error:", 6);
    if (!is_ok) ESP_LOGI(TAG, "< %s", line);     // komunikaty, błędy, alarmy, baner startowy
    if (is_ok || is_err || !strncmp(line, "ALARM:", 6)) {
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        if (is_err) s_st.last_error = atoi(line + 6);
        if (!is_ok && !is_err) s_st.alarm = atoi(line + 6);
        s_st.msg_seq++;
        xSemaphoreGive(s_mtx);
    }
    if (is_ok || is_err) {
        for (int i = 0; i < s_resp_n; i++) s_resp_cb[i](is_ok, is_err ? atoi(line + 6) : 0, s_resp_ctx[i]);
    }
    for (int i = 0; i < s_line_n; i++) s_line_cb[i](line, s_line_ctx[i]);
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
    int64_t t0 = esp_timer_get_time();
    int64_t t_full = 0;
    bool auto_mpg_done = false;
    for (;;) {
        // Pole MPG: jest tylko w raporcie po zmianie trybu albo w pełnym raporcie (0x87).
        // Dopóki stan nieznany (np. po restarcie panelu), co 3 s prosimy o pełny raport zamiast zwykłego.
        int64_t now = esp_timer_get_time();
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        bool need_full = s_st.valid && s_st.mpg < 0;
        xSemaphoreGive(s_mtx);
        if (need_full && now - t_full > 3000000LL) {
            t_full = now;
            grbl_send_realtime((char)0x87);
        } else {
            grbl_send_realtime('?');
        }
        // Poza trybem MPG grblHAL nie odpowiada na tym porcie – jeśli po starcie cisza, przejmij sterowanie raz.
        if (!auto_mpg_done && s_last_report_us == 0 && (esp_timer_get_time() - t0) > AUTO_MPG_MS * 1000LL) {
            auto_mpg_done = true;
            ESP_LOGW(TAG, "brak raportów - wysyłam 0x8B (przejęcie MPG)");
            grbl_send_realtime((char)0x8B);
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

void grbl_link_init(void)
{
    s_mtx = xSemaphoreCreateMutex();
    s_tx_mtx = xSemaphoreCreateMutex();

    const uart_config_t cfg = {
        .baud_rate = GRBL_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(GRBL_UART_NUM, 2048, 1024, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(GRBL_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(GRBL_UART_NUM, GRBL_UART_TX_GPIO, GRBL_UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(rx_task, "grbl_rx", 4096, NULL, 6, NULL);
    xTaskCreate(poll_task, "grbl_poll", 3072, NULL, 4, NULL);
    ESP_LOGI(TAG, "łącze grblHAL: UART%d %d baud, TX=%d RX=%d", GRBL_UART_NUM, GRBL_UART_BAUD,
             GRBL_UART_TX_GPIO, GRBL_UART_RX_GPIO);
}

void grbl_link_add_response_cb(grbl_resp_cb_t cb, void *ctx)
{
    if (s_resp_n < MAX_CB) { s_resp_cb[s_resp_n] = cb; s_resp_ctx[s_resp_n] = ctx; s_resp_n++; }
}

void grbl_link_add_line_cb(grbl_line_cb_t cb, void *ctx)
{
    if (s_line_n < MAX_CB) { s_line_cb[s_line_n] = cb; s_line_ctx[s_line_n] = ctx; s_line_n++; }
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
    char buf[200];
    size_t n = strlen(line);
    if (n > sizeof buf - 2) return false;
    memcpy(buf, line, n);
    buf[n++] = '\n';
    xSemaphoreTake(s_tx_mtx, portMAX_DELAY);
    int w = uart_write_bytes(GRBL_UART_NUM, buf, n);
    xSemaphoreGive(s_tx_mtx);
    return w == (int)n;
}

void grbl_send_realtime(char c)
{
    xSemaphoreTake(s_tx_mtx, portMAX_DELAY);
    uart_write_bytes(GRBL_UART_NUM, &c, 1);
    xSemaphoreGive(s_tx_mtx);
}

void grbl_mpg_toggle(void)
{
    grbl_send_realtime((char)0x8B);
}

void grbl_mpg_request(bool panel_controls)
{
    grbl_status_t st;
    grbl_link_get_status(&st);
    if (st.mpg < 0 || st.mpg != (panel_controls ? 1 : 0)) grbl_mpg_toggle();
}

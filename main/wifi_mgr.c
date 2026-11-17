#include "wifi_mgr.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "http_upload.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "wifi";

static wifi_info_t s_info;
static wifi_ap_t s_scan[WIFI_SCAN_MAX];
static int s_scan_n;
static SemaphoreHandle_t s_mtx;
static bool s_want_connect;     // true = próbuj łączyć ponownie po rozłączeniu

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            if (s_want_connect) esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *d = data;
            xSemaphoreTake(s_mtx, portMAX_DELAY);
            s_info.connected = false;
            s_info.ip[0] = 0;
            s_info.fail_reason = d->reason;
            s_info.connecting = s_want_connect;
            xSemaphoreGive(s_mtx);
            http_upload_stop();
            if (s_want_connect) { vTaskDelay(pdMS_TO_TICKS(1500)); esp_wifi_connect(); }
            break;
        }
        case WIFI_EVENT_SCAN_DONE: {
            uint16_t n = WIFI_SCAN_MAX;
            wifi_ap_record_t rec[WIFI_SCAN_MAX];
            if (esp_wifi_scan_get_ap_records(&n, rec) != ESP_OK) n = 0;
            xSemaphoreTake(s_mtx, portMAX_DELAY);
            s_scan_n = 0;
            for (int i = 0; i < n; i++) {
                if (!rec[i].ssid[0]) continue;
                strncpy(s_scan[s_scan_n].ssid, (char *)rec[i].ssid, 32);
                s_scan[s_scan_n].ssid[32] = 0;
                s_scan[s_scan_n].rssi = rec[i].rssi;
                s_scan[s_scan_n].secure = rec[i].authmode != WIFI_AUTH_OPEN;
                s_scan_n++;
            }
            s_info.scanning = false;
            xSemaphoreGive(s_mtx);
            break;
        }
        default: break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        s_info.connected = true;
        s_info.connecting = false;
        s_info.fail_reason = 0;
        snprintf(s_info.ip, sizeof s_info.ip, IPSTR, IP2STR(&e->ip_info.ip));
        xSemaphoreGive(s_mtx);
        ESP_LOGI(TAG, "połączono, IP %s", s_info.ip);
        http_upload_start();
    }
}

static bool load_creds(char *ssid, size_t sl, char *pass, size_t pl)
{
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) return false;
    bool ok = nvs_get_str(h, "ssid", ssid, &sl) == ESP_OK && nvs_get_str(h, "pass", pass, &pl) == ESP_OK;
    nvs_close(h);
    return ok && ssid[0];
}

static void save_creds(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READWRITE, &h) != ESP_OK) return;
    if (ssid) { nvs_set_str(h, "ssid", ssid); nvs_set_str(h, "pass", pass ? pass : ""); }
    else { nvs_erase_key(h, "ssid"); nvs_erase_key(h, "pass"); }
    nvs_commit(h);
    nvs_close(h);
}

static void apply_and_connect(const char *ssid, const char *pass)
{
    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, ssid, sizeof cfg.sta.ssid - 1);
    strncpy((char *)cfg.sta.password, pass, sizeof cfg.sta.password - 1);
    cfg.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    strncpy(s_info.ssid, ssid, sizeof s_info.ssid - 1);
    s_info.connecting = true;
    s_info.fail_reason = 0;
    xSemaphoreGive(s_mtx);
    s_want_connect = true;
    esp_wifi_connect();
}

// Inicjalizacja w osobnym zadaniu: esp_hosted czeka na C6 i może się zawiesić/zgłosić błąd
// (np. niezgodna wersja firmware slave) - interfejs ma wtedy dalej działać.
static void init_task(void *arg)
{
    esp_err_t err = esp_netif_init();
    if (err == ESP_OK) err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE) err = ESP_OK;   // pętla już istnieje
    if (err == ESP_OK) {
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&cfg);
    }
    if (err == ESP_OK) {
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL);
        esp_wifi_set_mode(WIFI_MODE_STA);
        char ssid[33] = "", pass[65] = "";
        bool have = load_creds(ssid, sizeof ssid, pass, sizeof pass);
        if (have) {
            wifi_config_t wc = {0};
            strncpy((char *)wc.sta.ssid, ssid, sizeof wc.sta.ssid - 1);
            strncpy((char *)wc.sta.password, pass, sizeof wc.sta.password - 1);
            esp_wifi_set_config(WIFI_IF_STA, &wc);
            strncpy(s_info.ssid, ssid, sizeof s_info.ssid - 1);
            s_want_connect = true;
            s_info.connecting = true;
        }
        err = esp_wifi_start();
    }
    if (err == ESP_OK) {
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        s_info.ready = true;
        xSemaphoreGive(s_mtx);
    } else {
        ESP_LOGE(TAG, "Wi-Fi niedostępne: %s (sprawdź firmware ESP32-C6)", esp_err_to_name(err));
    }
    vTaskDelete(NULL);
}

void wifi_mgr_init(void)
{
    s_mtx = xSemaphoreCreateMutex();
    xTaskCreate(init_task, "wifi_init", 6144, NULL, 5, NULL);
}

void wifi_mgr_info(wifi_info_t *out)
{
    if (!s_mtx) { memset(out, 0, sizeof *out); return; }
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    *out = s_info;
    xSemaphoreGive(s_mtx);
}

void wifi_mgr_scan(void)
{
    if (!s_info.ready || s_info.scanning) return;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_info.scanning = true;
    s_scan_n = 0;
    xSemaphoreGive(s_mtx);
    wifi_scan_config_t sc = {0};
    if (esp_wifi_scan_start(&sc, false) != ESP_OK) {
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        s_info.scanning = false;
        xSemaphoreGive(s_mtx);
    }
}

int wifi_mgr_scan_results(wifi_ap_t *out, int max)
{
    if (!s_mtx) return 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    int n = s_scan_n < max ? s_scan_n : max;
    memcpy(out, s_scan, n * sizeof out[0]);
    xSemaphoreGive(s_mtx);
    return n;
}

void wifi_mgr_connect(const char *ssid, const char *pass)
{
    if (!s_info.ready) return;
    save_creds(ssid, pass);
    apply_and_connect(ssid, pass ? pass : "");
}

void wifi_mgr_forget(void)
{
    s_want_connect = false;
    save_creds(NULL, NULL);
    if (s_info.ready) esp_wifi_disconnect();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_info.ssid[0] = 0;
    s_info.connecting = false;
    xSemaphoreGive(s_mtx);
}

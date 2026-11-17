#pragma once

#include <stdbool.h>
#include <stdint.h>

// Wi-Fi przez ESP32-C6 (ESP-Hosted po SDIO, API esp_wifi_remote). Wymaga, by firmware slave w C6
// był w wersji zgodnej z hostem (2.12.x) - patrz README.

#define WIFI_SCAN_MAX 16

typedef struct {
    char ssid[33];
    int8_t rssi;
    bool secure;
} wifi_ap_t;

typedef struct {
    bool ready;          // stos Wi-Fi uruchomiony (false = błąd inicjalizacji / brak C6)
    bool connecting;
    bool connected;
    bool scanning;
    char ssid[33];       // sieć, z którą połączono lub próbowano
    char ip[16];
    int fail_reason;     // ostatni powód rozłączenia (0 = brak)
} wifi_info_t;

void wifi_mgr_init(void);                       // uruchamia zadanie inicjalizujące, nie blokuje
void wifi_mgr_info(wifi_info_t *out);
void wifi_mgr_scan(void);                       // asynchronicznie
int  wifi_mgr_scan_results(wifi_ap_t *out, int max);   // liczba wyników (0 gdy brak)
void wifi_mgr_connect(const char *ssid, const char *pass);   // zapisuje dane w NVS
void wifi_mgr_forget(void);                     // rozłącza i kasuje zapisane dane

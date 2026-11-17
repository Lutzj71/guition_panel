#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "grbl_link.h"
#include "nvs_flash.h"
#include "sdcard.h"
#include "settings.h"
#include "ui.h"
#include "wifi_mgr.h"

static const char *TAG = "main";

static void sd_task(void *arg)
{
    sdcard_mount();     // ekran Pliki dociąga kartę ponownie, jeśli ją włożono później
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    lv_disp_t *disp = board_init();
    if (!disp) {
        ESP_LOGE(TAG, "inicjalizacja wyświetlacza nie powiodła się");
        return;
    }
    grbl_link_init();
    settings_init();
    ui_create(disp);

    xTaskCreate(sd_task, "sd_mount", 4096, NULL, 3, NULL);
    wifi_mgr_init();     // w osobnym zadaniu - nie blokuje interfejsu, gdy C6 nie odpowiada
}

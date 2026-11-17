#include "board.h"
#include "esp_log.h"
#include "grbl_link.h"
#include "ui.h"

static const char *TAG = "main";

void app_main(void)
{
    lv_disp_t *disp = board_init();
    if (!disp) {
        ESP_LOGE(TAG, "inicjalizacja wyświetlacza nie powiodła się");
        return;
    }
    grbl_link_init();
    ui_create(disp);
}

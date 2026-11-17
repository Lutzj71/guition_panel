#include "sdcard.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"

static const char *TAG = "sd";
static sdmmc_card_t *s_card;

// Piny SDMMC slot 0 (stałe na ESP32-P4): CLK 43, CMD 44, D0..D3 = 39..42.
bool sdcard_mount(void)
{
    if (s_card) return true;

    // TF_VCC jest zasilane z wewnętrznego LDO VO4 (3,3 V) - bez tego karta nie odpowiada (send_op_cond 0x107).
    sd_pwr_ctrl_handle_t pwr = NULL;
    sd_pwr_ctrl_ldo_config_t ldo = {.ldo_chan_id = 4};
    esp_err_t err = sd_pwr_ctrl_new_on_chip_ldo(&ldo, &pwr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LDO VO4: %s", esp_err_to_name(err));
        return false;
    }

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    host.pwr_ctrl_handle = pwr;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
#if SOC_SDMMC_USE_GPIO_MATRIX
    slot.clk = GPIO_NUM_43;
    slot.cmd = GPIO_NUM_44;
    slot.d0 = GPIO_NUM_39;
    slot.d1 = GPIO_NUM_40;
    slot.d2 = GPIO_NUM_41;
    slot.d3 = GPIO_NUM_42;
#endif

    esp_vfs_fat_mount_config_t mc = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };
    err = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot, &mc, &s_card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "montowanie karty: %s", esp_err_to_name(err));
        s_card = NULL;
        return false;
    }
    ESP_LOGI(TAG, "karta zamontowana: %s", s_card->cid.name);
    return true;
}

bool sdcard_mounted(void) { return s_card != NULL; }

static bool is_gcode(const char *n)
{
    const char *dot = strrchr(n, '.');
    if (!dot || n[0] == '.') return false;
    static const char *ext[] = {".nc", ".gcode", ".gc", ".ngc", ".tap", ".txt"};
    for (size_t i = 0; i < sizeof ext / sizeof ext[0]; i++)
        if (!strcasecmp(dot, ext[i])) return true;
    return false;
}

static int cmp_name(const void *a, const void *b)
{
    return strcasecmp(((const sd_file_t *)a)->name, ((const sd_file_t *)b)->name);
}

int sdcard_list(sd_file_t *out, int max)
{
    if (!s_card) return 0;
    DIR *d = opendir(SD_MOUNT_POINT);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while (n < max && (e = readdir(d))) {
        if (e->d_type == DT_DIR || strlen(e->d_name) >= SD_NAME_MAX || !is_gcode(e->d_name)) continue;
        char path[SD_NAME_MAX + 16];
        struct stat st;
        snprintf(path, sizeof path, SD_MOUNT_POINT "/%s", e->d_name);
        if (stat(path, &st) != 0) continue;
        strcpy(out[n].name, e->d_name);
        out[n].size = (uint32_t)st.st_size;
        n++;
    }
    closedir(d);
    qsort(out, n, sizeof out[0], cmp_name);
    return n;
}

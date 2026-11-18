#include "sdcard.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"

#define WATCH_PERIOD_MS 2000

static const char *TAG = "sd";
static sdmmc_card_t *s_card;
static sd_pwr_ctrl_handle_t s_pwr;
static SemaphoreHandle_t s_mtx;     // montowanie, odmontowanie, listowanie, licznik blokad
static int s_holds;
static volatile uint32_t s_gen;

static void lock(void)
{
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();     // pierwsze wywołanie jest w app_main (sdcard_init)
    xSemaphoreTake(s_mtx, portMAX_DELAY);
}
static void unlock(void) { xSemaphoreGive(s_mtx); }

// Piny SDMMC slot 0 (stałe na ESP32-P4): CLK 43, CMD 44, D0..D3 = 39..42.
static bool mount_locked(void)
{
    if (s_card) return true;

    // TF_VCC jest zasilane z wewnętrznego LDO VO4 (3,3 V) - bez tego karta nie odpowiada (send_op_cond 0x107).
    // Uchwyt LDO trzeba zwolnić przy każdej nieudanej próbie, inaczej kolejne montowanie nie dostanie kanału.
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
        ESP_LOGD(TAG, "montowanie karty: %s", esp_err_to_name(err));
        s_card = NULL;
        sd_pwr_ctrl_del_on_chip_ldo(pwr);
        return false;
    }
    s_pwr = pwr;
    s_gen++;
    ESP_LOGI(TAG, "karta zamontowana: %s", s_card->cid.name);
    return true;
}

static void unmount_locked(void)
{
    if (!s_card) return;
    esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_card);
    s_card = NULL;
    if (s_pwr) { sd_pwr_ctrl_del_on_chip_ldo(s_pwr); s_pwr = NULL; }
    s_gen++;
    ESP_LOGW(TAG, "karta wyjęta - odmontowano");
}

bool sdcard_mount(void)
{
    lock();
    bool ok = mount_locked();
    unlock();
    return ok;
}

bool sdcard_mounted(void) { return s_card != NULL; }
uint32_t sdcard_generation(void) { return s_gen; }

bool sdcard_hold(void)
{
    lock();
    bool ok = s_card != NULL;
    if (ok) s_holds++;
    unlock();
    return ok;
}

void sdcard_release(void)
{
    lock();
    if (s_holds > 0) s_holds--;
    unlock();
}

static void watch_task(void *arg)
{
    lock();
    bool ok = mount_locked();
    unlock();
    if (!ok) ESP_LOGW(TAG, "brak karty SD - czekam na włożenie");

    // Kolejne próby montowania bez karty co 2 s zalewałyby log błędami sterownika - wyciszamy je.
    static const char *quiet[] = {"sdmmc_common", "sdmmc_init", "sdmmc_sd", "sdmmc_req", "vfs_fat_sdmmc", "ldo"};
    for (size_t i = 0; i < sizeof quiet / sizeof quiet[0]; i++) esp_log_level_set(quiet[i], ESP_LOG_NONE);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(WATCH_PERIOD_MS));
        lock();
        if (s_holds == 0) {
            if (s_card) {
                if (sdmmc_get_status(s_card) != ESP_OK) unmount_locked();
            } else {
                mount_locked();
            }
        }
        unlock();
    }
}

void sdcard_init(void)
{
    lock();      // tworzy mutex w kontekście app_main, zanim ktokolwiek inny sięgnie po kartę
    unlock();
    xTaskCreate(watch_task, "sd_watch", 4096, NULL, 3, NULL);
}

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
    const sd_file_t *x = a, *y = b;
    if (x->is_dir != y->is_dir) return x->is_dir ? -1 : 1;     // katalogi na początku
    return strcasecmp(x->name, y->name);
}

static int list_locked(const char *subdir, sd_file_t *out, int max);

int sdcard_list(const char *subdir, sd_file_t *out, int max)
{
    lock();      // nadzór nie odmontuje karty w trakcie czytania katalogu
    int n = list_locked(subdir, out, max);
    unlock();
    return n;
}

static int list_locked(const char *subdir, sd_file_t *out, int max)
{
    if (!s_card) return 0;
    char dir[sizeof(SD_MOUNT_POINT) + SD_PATH_MAX + 2];
    if (subdir && subdir[0]) snprintf(dir, sizeof dir, SD_MOUNT_POINT "/%.*s", SD_PATH_MAX - 1, subdir);
    else snprintf(dir, sizeof dir, "%s", SD_MOUNT_POINT);
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while (n < max && (e = readdir(d))) {
        bool is_dir = e->d_type == DT_DIR;
        if (e->d_name[0] == '.' || strlen(e->d_name) >= SD_NAME_MAX) continue;
        if (is_dir && !strcasecmp(e->d_name, "System Volume Information")) continue;
        if (!is_dir && !is_gcode(e->d_name)) continue;
        uint32_t size = 0;
        if (!is_dir) {
            char path[sizeof dir + 256];
            struct stat st;
            snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
            if (stat(path, &st) != 0) continue;
            size = (uint32_t)st.st_size;
        }
        strcpy(out[n].name, e->d_name);
        out[n].size = size;
        out[n].is_dir = is_dir;
        n++;
    }
    closedir(d);
    qsort(out, n, sizeof out[0], cmp_name);
    return n;
}

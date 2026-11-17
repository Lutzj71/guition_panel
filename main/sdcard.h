#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SD_MOUNT_POINT "/sdcard"
#define SD_NAME_MAX    64       // pojedyncza nazwa pliku / katalogu
#define SD_PATH_MAX    128      // ścieżka względem karty: "katalog/podkatalog/plik.nc"

typedef struct {
    char name[SD_NAME_MAX];
    uint32_t size;
    bool is_dir;
} sd_file_t;

// Montuje kartę (SDMMC 4-bit, zasilanie z LDO VO4). Zwraca true, gdy karta jest dostępna.
bool sdcard_mount(void);
bool sdcard_mounted(void);
// Zawartość katalogu `subdir` (ścieżka względem karty, "" = katalog główny): najpierw podkatalogi,
// potem pliki G-code, każda grupa alfabetycznie. Ukryte wpisy (od kropki) są pomijane.
int sdcard_list(const char *subdir, sd_file_t *out, int max);

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SD_MOUNT_POINT "/sdcard"
#define SD_NAME_MAX    64

typedef struct {
    char name[SD_NAME_MAX];
    uint32_t size;
} sd_file_t;

// Montuje kartę (SDMMC 4-bit, zasilanie z LDO VO4). Zwraca true, gdy karta jest dostępna.
bool sdcard_mount(void);
bool sdcard_mounted(void);
// Pliki G-code z katalogu głównego karty, posortowane alfabetycznie. Zwraca liczbę wpisów.
int sdcard_list(sd_file_t *out, int max);

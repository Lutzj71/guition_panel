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

// Uruchamia zadanie nadzoru karty: pierwsze montowanie od razu, potem co ~2 s sprawdzanie obecności
// (CMD13 przy zamontowanej karcie, próba montowania przy jej braku). Wyjęcie/włożenie karty jest
// wykrywane automatycznie; w czasie, gdy ktoś trzyma kartę (sdcard_hold), nadzór niczego nie robi.
void sdcard_init(void);
// Montuje kartę (SDMMC 4-bit, zasilanie z LDO VO4). Zwraca true, gdy karta jest dostępna.
bool sdcard_mount(void);
bool sdcard_mounted(void);
// Licznik zmian stanu karty (każde zamontowanie i odmontowanie) - do odświeżania widoków.
uint32_t sdcard_generation(void);
// Blokuje odmontowanie na czas operacji na plikach (od fopen do fclose). false, gdy karty nie ma.
bool sdcard_hold(void);
void sdcard_release(void);
// Zawartość katalogu `subdir` (ścieżka względem karty, "" = katalog główny): najpierw podkatalogi,
// potem pliki G-code, każda grupa alfabetycznie. Ukryte wpisy (od kropki) są pomijane.
int sdcard_list(const char *subdir, sd_file_t *out, int max);

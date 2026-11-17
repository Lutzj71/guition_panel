#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sdcard.h"

// Strumieniowanie pliku G-code z karty SD do grblHAL (character counting, okno 120 B).

typedef enum { GS_IDLE, GS_RUNNING, GS_PAUSED, GS_DONE, GS_ERROR, GS_ABORTED } gs_state_t;

typedef struct {
    gs_state_t state;
    char name[SD_PATH_MAX];   // ścieżka względem karty
    uint32_t bytes_total, bytes_done;
    uint32_t lines_sent, lines_acked;
    int error;              // numer błędu grblHAL przy GS_ERROR
    char error_line[96];    // linia, która go wywołała
    char message[64];       // opis problemu po stronie panelu (np. za długa linia)
} gs_status_t;

// Plik na karcie: ścieżka względem karty ("plik.nc" lub "katalog/plik.nc"). Zwraca false i wpisuje powód do *why przy odmowie.
bool gcode_stream_start(const char *name, const char **why);
void gcode_stream_pause(void);
void gcode_stream_resume(void);
void gcode_stream_stop(void);          // feed hold + reset (wyłącza laser)
void gcode_stream_get(gs_status_t *out);
bool gcode_stream_active(void);        // RUNNING lub PAUSED lub ERROR (zadanie jeszcze nie zamknięte)
void gcode_stream_dismiss(void);       // po DONE / ERROR / ABORTED wraca do IDLE

#pragma once

#include "grbl_link.h"
#include "lvgl.h"
#include "settings.h"

// Ekrany zgodne z makietą (Main, Jog, Run, Settings, Edit, Alarm) + Pliki (karta SD) i Wi-Fi.
typedef enum {
    SCR_MAIN, SCR_JOG, SCR_RUN, SCR_SETTINGS, SCR_EDIT, SCR_ALARM, SCR_FILES, SCR_WIFI, SCR_COUNT
} ui_scr_t;

void ui_create(lv_disp_t *disp);
void ui_goto(ui_scr_t id);
ui_scr_t ui_current(void);

// Uruchamia edycję ustawienia (ekran SCR_EDIT) - wywołać przed ui_goto(SCR_EDIT).
void ui_edit_set_target(const set_def_t *d);

// Komunikat chwilowo na dole ekranu (tylko z wątku LVGL).
void ui_toast(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Czy panel może wysyłać komendy (tryb MPG nie jest jawnie wyłączony). Przy odmowie pokazuje komunikat.
bool ui_can_command(const grbl_status_t *st);

// Każdy ekran: tworzy obiekty i zwraca ekran; update wywoływany co ~150 ms dla aktywnego.
lv_obj_t *ui_main_create(void);     void ui_main_update(const grbl_status_t *st);
lv_obj_t *ui_jog_create(void);      void ui_jog_update(const grbl_status_t *st);
lv_obj_t *ui_run_create(void);      void ui_run_update(const grbl_status_t *st);
lv_obj_t *ui_settings_create(void); void ui_settings_update(const grbl_status_t *st);
lv_obj_t *ui_edit_create(void);     void ui_edit_update(const grbl_status_t *st);
lv_obj_t *ui_alarm_create(void);    void ui_alarm_update(const grbl_status_t *st);
lv_obj_t *ui_files_create(void);    void ui_files_update(const grbl_status_t *st);
lv_obj_t *ui_wifi_create(void);     void ui_wifi_update(const grbl_status_t *st);

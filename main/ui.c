#include "ui.h"

#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "gcode_stream.h"
#include "ui_common.h"

static ui_scr_t s_cur = SCR_COUNT;
static int64_t s_entered_us;

typedef lv_obj_t *(*create_fn)(void);
typedef void (*update_fn)(const grbl_status_t *);

static const struct { create_fn create; update_fn update; } SCREENS[SCR_COUNT] = {
    [SCR_MAIN]     = {ui_main_create, ui_main_update},
    [SCR_JOG]      = {ui_jog_create, ui_jog_update},
    [SCR_RUN]      = {ui_run_create, ui_run_update},
    [SCR_SETTINGS] = {ui_settings_create, ui_settings_update},
    [SCR_EDIT]     = {ui_edit_create, ui_edit_update},
    [SCR_ALARM]    = {ui_alarm_create, ui_alarm_update},
    [SCR_FILES]    = {ui_files_create, ui_files_update},
    [SCR_WIFI]     = {ui_wifi_create, ui_wifi_update},
};

void ui_goto(ui_scr_t id)
{
    if (id >= SCR_COUNT) return;
    lv_obj_t *s = SCREENS[id].create();
    s_cur = id;
    s_entered_us = esp_timer_get_time();
    lv_scr_load_anim(s, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

ui_scr_t ui_current(void) { return s_cur; }

// Automatyczne przełączanie ekranów według stanu maszyny i zadania.
static void auto_navigate(const grbl_status_t *st)
{
    bool in_alarm = !st->stale && st->state_id == GRBL_ST_ALARM;
    bool job = gcode_stream_active() ||
               (!st->stale && (st->state_id == GRBL_ST_RUN || st->state_id == GRBL_ST_HOLD || st->state_id == GRBL_ST_DOOR));
    bool idle_for_nav = !job && !in_alarm;

    switch (s_cur) {
    case SCR_ALARM:
        if (!in_alarm) ui_goto(SCR_MAIN);
        break;
    case SCR_RUN:
        if (in_alarm) ui_goto(SCR_ALARM);
        else if (idle_for_nav && (esp_timer_get_time() - s_entered_us) > 1500000) {
            gs_status_t gs;
            gcode_stream_get(&gs);
            if (gs.state == GS_IDLE) ui_goto(SCR_MAIN);   // po DONE / ABORTED ekran Pracy zostaje do zamknięcia
        }
        break;
    case SCR_MAIN:
    case SCR_JOG:
    case SCR_FILES:
        if (in_alarm) ui_goto(SCR_ALARM);
        else if (job) ui_goto(SCR_RUN);
        break;
    default:
        // Ustawienia / Edycja / Wi-Fi: przy alarmie tylko ustawienia są dozwolone (zmiana wymaga Idle lub Alarm)
        if (job && s_cur != SCR_SETTINGS && s_cur != SCR_EDIT) ui_goto(SCR_RUN);
        break;
    }
}

static void tick(lv_timer_t *t)
{
    grbl_status_t st;
    grbl_link_get_status(&st);
    auto_navigate(&st);
    if (s_cur < SCR_COUNT) SCREENS[s_cur].update(&st);
}

void ui_create(lv_disp_t *disp)
{
    lvgl_port_lock(0);
    ui_fonts_init();
    ui_goto(SCR_MAIN);
    lv_timer_create(tick, 150, NULL);
    lvgl_port_unlock();
}

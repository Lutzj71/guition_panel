#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdcard.h"
#include "ui_common.h"
#include "wifi_mgr.h"

// ================================================================= Ustawienia

static int s_cat = SC_LASER;
static lv_obj_t *list, *tab_btn[SC_COUNT], *status_lbl;
static uint32_t s_sig;
static grbl_status_t cur;

static bool can_change(void)
{
    if (cur.stale) { ui_toast("Brak łącza z maszyną"); return false; }
    if (cur.mpg == 0) { ui_toast("Sterowanie ma komputer"); return false; }
    if (cur.state_id != GRBL_ST_IDLE && cur.state_id != GRBL_ST_ALARM) {
        ui_toast("Zmiany tylko w stanie Idle lub Alarm");
        return false;
    }
    return true;
}

static void rebuild(void);

static void row_cb(lv_event_t *e)
{
    const set_def_t *d = lv_event_get_user_data(e);
    if (d->kind == SK_NUM) {
        ui_edit_set_target(d);
        ui_goto(SCR_EDIT);
        return;
    }
    if (!can_change()) return;
    float v;
    if (!settings_get(d, &v)) { ui_toast("Ustawienia nie zostały jeszcze wczytane"); return; }
    settings_write(d, v != 0 ? 0 : 1);
    rebuild();
}

static void panel_row_cb(lv_event_t *e)
{
    switch ((int)(intptr_t)lv_event_get_user_data(e)) {
    case 0: ui_goto(SCR_WIFI); break;
    case 2:
        if (cur.stale) { ui_toast("Brak łącza z maszyną"); break; }
        settings_refresh();
        ui_toast("Wczytuję ustawienia z maszyny…");
        break;
    default: break;
    }
}

static lv_obj_t *make_row(const char *label, const char *sub, lv_event_cb_t cb, void *user)
{
    lv_obj_t *r = ui_button(list, NULL, ui_f20, cb, user);
    lv_obj_remove_style(r, NULL, LV_PART_MAIN);   // zdejmij styl przycisku, zostaw tylko własny
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, 84);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(r, C_BTN, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, C_DIV, 0);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_set_style_text_color(r, C_TEXT, 0);
    lv_obj_set_style_pad_hor(r, 0, 0);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, 14, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_t *c = ui_col(r);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_width(c, 0);
    ui_label(c, label, ui_f20, C_TEXT);
    ui_label(c, sub, ui_f16, C_MUTED);
    return r;
}

static void add_value(lv_obj_t *row, const char *val, const char *unit, bool chevron)
{
    lv_obj_t *v = ui_label(row, val, ui_f24, C_TEXT);
    (void)v;
    if (unit && unit[0]) ui_label(row, unit, ui_f16, C_MUTED);
    if (chevron) ui_label(row, LV_SYMBOL_RIGHT, ui_f20, C_DIM);
}

static void add_switch(lv_obj_t *row, bool on)
{
    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 64, 36);
    lv_obj_clear_flag(sw, LV_OBJ_FLAG_CLICKABLE);     // klika cały wiersz
    lv_obj_set_style_bg_color(sw, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, C_AMBER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, C_MUTED, LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, C_ON_AMBER, LV_PART_KNOB | LV_STATE_CHECKED);
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
}

static void rebuild(void)
{
    lv_obj_clean(list);
    if (s_cat != SC_PANEL) {
        for (int i = 0; i < SET_DEF_COUNT; i++) {
            const set_def_t *d = &SET_DEFS[i];
            if (d->cat != s_cat) continue;
            char sub[24], val[24] = "–";
            if (d->kind == SK_BIT && d->bit) snprintf(sub, sizeof sub, "$%u · bit %u", d->id, d->bit);
            else snprintf(sub, sizeof sub, "$%u", d->id);
            float v;
            bool have = settings_get(d, &v);
            if (have && d->kind == SK_NUM) snprintf(val, sizeof val, "%g", (double)v);
            lv_obj_t *r = make_row(d->label, sub, row_cb, (void *)d);
            if (d->kind == SK_BIT) add_switch(r, have && v != 0);
            else add_value(r, val, d->unit, true);
        }
        return;
    }
    wifi_info_t wi;
    wifi_mgr_info(&wi);
    const char *w = !wi.ready ? "niedostępne" : wi.connected ? wi.ssid : wi.connecting ? "łączenie…" : "rozłączone";
    lv_obj_t *r = make_row("Wi-Fi", "sieć i wgrywanie plików", panel_row_cb, (void *)0);
    add_value(r, w, "", true);
    r = make_row("Karta SD", "pliki G-code", panel_row_cb, (void *)1);
    add_value(r, sdcard_mounted() ? "gotowa" : "brak", "", false);
    r = make_row("Wczytaj z maszyny", "odczyt $$", panel_row_cb, (void *)2);
    add_value(r, LV_SYMBOL_REFRESH, "", false);
}

static void tab_cb(lv_event_t *e)
{
    s_cat = (int)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < SC_COUNT; i++) ui_button_selected(tab_btn[i], i == s_cat);
    s_sig = 0;
}

lv_obj_t *ui_settings_create(void)
{
    lv_obj_t *scr = ui_new_screen();
    lv_obj_t *right;
    ui_header(scr, "Ustawienia", SCR_MAIN, &right);
    status_lbl = ui_label(right, "", ui_f16, C_MUTED);

    static lv_coord_t c3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t r2[] = {64, 64, LV_GRID_TEMPLATE_LAST};
    lv_obj_t *tabs = lv_obj_create(scr);
    lv_obj_remove_style_all(tabs);
    lv_obj_set_width(tabs, LV_PCT(100));
    lv_obj_set_height(tabs, 136);
    lv_obj_set_style_pad_gap(tabs, 8, 0);
    lv_obj_set_layout(tabs, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(tabs, c3, r2);
    for (int i = 0; i < SC_COUNT; i++) {
        tab_btn[i] = ui_button(tabs, SET_CAT_NAMES[i], ui_f20, tab_cb, (void *)(intptr_t)i);
        lv_obj_set_grid_cell(tab_btn[i], LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
        ui_button_selected(tab_btn[i], i == s_cat);
    }

    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_hor(card, 18, 0);
    lv_obj_set_style_pad_ver(card, 2, 0);
    lv_obj_set_scroll_dir(card, LV_DIR_VER);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    list = card;

    lv_obj_t *hint = ui_label(scr, "Zmiany można wprowadzać tylko, gdy maszyna stoi (Idle) albo jest w stanie alarmu.", ui_f16, C_MUTED);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);

    s_sig = 0;
    if (!settings_loaded()) settings_refresh();
    return scr;
}

void ui_settings_update(const grbl_status_t *st)
{
    cur = *st;
    lv_label_set_text(status_lbl, settings_loaded() ? LV_SYMBOL_OK " Zapisane w EEPROM" : "Wczytywanie…");
    lv_obj_set_style_text_color(status_lbl, settings_loaded() ? C_GREEN : C_MUTED, 0);

    int err = settings_take_error();
    if (err) ui_toast("Maszyna odrzuciła zmianę (error:%d)", err);

    uint32_t sig = settings_version() * 31 + (uint32_t)s_cat;
    if (s_cat == SC_PANEL) {
        wifi_info_t wi;
        wifi_mgr_info(&wi);
        sig = sig * 31 + wi.ready + 2 * wi.connected + 4 * wi.connecting + 8 * sdcard_mounted() + (uint32_t)strlen(wi.ssid) * 16;
    }
    if (sig != s_sig) { s_sig = sig; rebuild(); }
}

// ================================================================= Edycja wartości

static const set_def_t *s_target;
static char s_buf[16];
static lv_obj_t *val_lbl;

void ui_edit_set_target(const set_def_t *d)
{
    s_target = d;
    s_buf[0] = 0;
}

static void key_cb(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    size_t n = strlen(s_buf);
    if (k == 'B') { if (n) s_buf[n - 1] = 0; }
    else if (k == '.') { if (n && !strchr(s_buf, '.') && n < sizeof s_buf - 1) { s_buf[n] = '.'; s_buf[n + 1] = 0; } }
    else if (n < sizeof s_buf - 1) {
        if (!(n == 1 && s_buf[0] == '0' && k == '0')) { s_buf[n] = (char)k; s_buf[n + 1] = 0; }
    }
    lv_label_set_text(val_lbl, s_buf[0] ? s_buf : "–");
}

static void save_cb(lv_event_t *e)
{
    if (!s_buf[0]) { ui_toast("Wpisz wartość"); return; }
    if (!can_change()) return;
    if (!settings_write(s_target, (float)atof(s_buf))) { ui_toast("Nie udało się wysłać"); return; }
    ui_toast("Wysłano $%u=%s", s_target->id, s_buf);
    ui_goto(SCR_SETTINGS);
}

static void cancel_cb(lv_event_t *e) { ui_goto(SCR_SETTINGS); }

lv_obj_t *ui_edit_create(void)
{
    lv_obj_t *scr = ui_new_screen();
    if (!s_target) s_target = &SET_DEFS[1];
    lv_obj_t *right;
    ui_header(scr, s_target->label, SCR_SETTINGS, &right);
    char id[16];
    snprintf(id, sizeof id, "$%u", s_target->id);
    ui_label(right, id, ui_f20, C_MUTED);

    lv_obj_t *card = ui_card(scr);
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, C_AMBER, 0);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    ui_label(card, "NOWA WARTOŚĆ", ui_f16, C_MUTED);
    lv_obj_t *vr = ui_row(card, 0);
    lv_obj_set_flex_align(vr, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    val_lbl = ui_label(vr, s_buf[0] ? s_buf : "–", ui_f56, C_TEXT);
    if (s_target->unit[0]) ui_label(vr, s_target->unit, ui_f20, C_MUTED);
    float v;
    char now[48] = "Teraz: –";
    if (settings_get(s_target, &v)) snprintf(now, sizeof now, "Teraz: %g %s", (double)v, s_target->unit);
    ui_label(card, now, ui_f16, C_MUTED);

    static lv_coord_t c3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t r4[] = {88, 88, 88, 88, LV_GRID_TEMPLATE_LAST};
    lv_obj_t *pad = lv_obj_create(scr);
    lv_obj_remove_style_all(pad);
    lv_obj_set_width(pad, LV_PCT(100));
    lv_obj_set_height(pad, 88 * 4 + 8 * 3);
    lv_obj_set_style_pad_gap(pad, 8, 0);
    lv_obj_set_layout(pad, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(pad, c3, r4);
    static const char keys[12] = {'7', '8', '9', '4', '5', '6', '1', '2', '3', '.', '0', 'B'};
    for (int i = 0; i < 12; i++) {
        char t[2] = {keys[i], 0};
        lv_obj_t *b = ui_button(pad, keys[i] == 'B' ? LV_SYMBOL_BACKSPACE : t, ui_f32, key_cb, (void *)(intptr_t)keys[i]);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
        if (keys[i] == '.' || keys[i] == 'B') lv_obj_set_style_bg_color(b, C_CARD, 0);
    }

    lv_obj_t *act = ui_row(scr, 84);
    lv_obj_t *c = ui_button(act, "Anuluj", ui_f20, cancel_cb, NULL);
    lv_obj_set_flex_grow(c, 10); lv_obj_set_height(c, 84);
    lv_obj_t *s = ui_button(act, "Zapisz", ui_f20, save_cb, NULL);
    lv_obj_set_flex_grow(s, 14); lv_obj_set_height(s, 84);
    ui_button_accent(s);
    return scr;
}

void ui_edit_update(const grbl_status_t *st)
{
    cur = *st;
}

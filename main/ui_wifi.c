#include <stdio.h>
#include <string.h>

#include "ui_common.h"
#include "wifi_mgr.h"

static lv_obj_t *status_lbl, *ip_lbl, *list, *scan_btn, *forget_btn;
static lv_obj_t *dlg, *ta, *kb;
static wifi_ap_t aps[WIFI_SCAN_MAX];
static char chosen[33];
static uint32_t s_sig;
static bool s_scan_requested;

static void close_dialog(void)
{
    if (dlg) { lv_obj_del(dlg); dlg = NULL; }
}

static void kb_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_CANCEL) close_dialog();
    else if (c == LV_EVENT_READY) {
        wifi_mgr_connect(chosen, lv_textarea_get_text(ta));
        close_dialog();
        ui_toast("Łączenie z %s…", chosen);
    }
}

static void show_dialog(const char *ssid, bool secure)
{
    strncpy(chosen, ssid, sizeof chosen - 1);
    chosen[sizeof chosen - 1] = 0;
    if (!secure) {                       // sieć otwarta - bez hasła
        wifi_mgr_connect(chosen, "");
        ui_toast("Łączenie z %s…", chosen);
        return;
    }
    dlg = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(dlg);
    lv_obj_set_size(dlg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(dlg, C_BG, 0);
    lv_obj_set_style_bg_opa(dlg, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(dlg, 16, 0);
    lv_obj_set_style_pad_row(dlg, 12, 0);
    lv_obj_set_flex_flow(dlg, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE);

    ui_label(dlg, "Hasło do sieci", ui_f20, C_MUTED);
    lv_obj_t *n = ui_label(dlg, chosen, ui_f32, C_AMBER);
    lv_obj_set_width(n, LV_PCT(100));
    lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);

    ta = lv_textarea_create(dlg);
    lv_obj_set_width(ta, LV_PCT(100));
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_max_length(ta, 63);
    lv_obj_set_style_text_font(ta, ui_f24, 0);
    lv_obj_set_style_bg_color(ta, C_CARD, 0);
    lv_obj_set_style_text_color(ta, C_TEXT, 0);

    lv_obj_t *sp = lv_obj_create(dlg);       // rozpórka dociskająca klawiaturę do dołu
    lv_obj_remove_style_all(sp);
    lv_obj_set_width(sp, 1);
    lv_obj_set_flex_grow(sp, 1);

    kb = lv_keyboard_create(dlg);
    lv_obj_set_size(kb, LV_PCT(100), 360);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_add_event_cb(kb, kb_cb, LV_EVENT_ALL, NULL);
}

static void ap_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (dlg || i < 0 || i >= WIFI_SCAN_MAX) return;
    show_dialog(aps[i].ssid, aps[i].secure);
}

static void scan_cb(lv_event_t *e)
{
    wifi_info_t wi;
    wifi_mgr_info(&wi);
    if (!wi.ready) { ui_toast("Wi-Fi niedostępne (sprawdź firmware ESP32-C6)"); return; }
    s_scan_requested = true;
    wifi_mgr_scan();
}

static void forget_cb(lv_event_t *e)
{
    wifi_mgr_forget();
    ui_toast("Rozłączono i zapomniano sieć");
}

static void rebuild_list(void)
{
    lv_obj_clean(list);
    int n = wifi_mgr_scan_results(aps, WIFI_SCAN_MAX);
    if (n == 0) {
        lv_obj_t *l = ui_label(list, "Dotknij „Szukaj sieci”.", ui_f20, C_MUTED);
        lv_obj_set_style_pad_all(l, 24, 0);
        return;
    }
    for (int i = 0; i < n; i++) {
        lv_obj_t *r = ui_button(list, NULL, ui_f20, ap_cb, (void *)(intptr_t)i);
        lv_obj_set_width(r, LV_PCT(100));
        lv_obj_set_height(r, 72);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_hor(r, 18, 0);
        lv_obj_set_style_pad_column(r, 12, 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(r, 0, 0);
        lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(r, 1, 0);
        lv_obj_set_style_border_color(r, C_DIV, 0);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        ui_label(r, LV_SYMBOL_WIFI, ui_f20, C_AMBER);
        lv_obj_t *s = ui_label(r, aps[i].ssid, ui_f20, C_TEXT);
        lv_obj_set_flex_grow(s, 1);
        lv_label_set_long_mode(s, LV_LABEL_LONG_DOT);
        char b[16];
        snprintf(b, sizeof b, "%d dBm", aps[i].rssi);
        ui_label(r, b, ui_f16, C_MUTED);
        if (aps[i].secure) ui_label(r, LV_SYMBOL_EYE_CLOSE, ui_f16, C_DIM);
    }
}

lv_obj_t *ui_wifi_create(void)
{
    lv_obj_t *scr = ui_new_screen();
    ui_header(scr, "Wi-Fi", SCR_SETTINGS, NULL);

    lv_obj_t *card = ui_card(scr);
    status_lbl = ui_label(card, "", ui_f24, C_TEXT);
    ip_lbl = ui_label(card, "", ui_f16, C_MUTED);
    lv_obj_set_width(ip_lbl, LV_PCT(100));
    lv_label_set_long_mode(ip_lbl, LV_LABEL_LONG_WRAP);

    lv_obj_t *row = ui_row(scr, 68);
    scan_btn = ui_button(row, LV_SYMBOL_REFRESH "  Szukaj sieci", ui_f20, scan_cb, NULL);
    lv_obj_set_flex_grow(scan_btn, 3); lv_obj_set_height(scan_btn, 68);
    ui_button_accent(scan_btn);
    forget_btn = ui_button(row, "Zapomnij", ui_f20, forget_cb, NULL);
    lv_obj_set_flex_grow(forget_btn, 2); lv_obj_set_height(forget_btn, 68);

    lv_obj_t *l = lv_obj_create(scr);
    lv_obj_remove_style_all(l);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_flex_grow(l, 1);
    lv_obj_set_style_bg_color(l, C_CARD, 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(l, 16, 0);
    lv_obj_set_scroll_dir(l, LV_DIR_VER);
    lv_obj_set_flex_flow(l, LV_FLEX_FLOW_COLUMN);
    list = l;

    dlg = NULL;
    s_sig = 0;
    s_scan_requested = false;
    rebuild_list();
    return scr;
}

void ui_wifi_update(const grbl_status_t *st)
{
    wifi_info_t wi;
    wifi_mgr_info(&wi);
    char b[128];

    if (!wi.ready) {
        lv_label_set_text(status_lbl, "Wi-Fi niedostępne");
        lv_label_set_text(ip_lbl, "Moduł ESP32-C6 nie odpowiada albo wersja jego firmware nie pasuje do hosta (patrz README).");
    } else if (wi.connected) {
        snprintf(b, sizeof b, LV_SYMBOL_WIFI "  %s", wi.ssid);
        lv_label_set_text(status_lbl, b);
        snprintf(b, sizeof b, "Adres IP: %s\nWgrywanie plików: http://%s/", wi.ip, wi.ip);
        lv_label_set_text(ip_lbl, b);
    } else if (wi.connecting) {
        snprintf(b, sizeof b, "Łączenie z %s…", wi.ssid);
        lv_label_set_text(status_lbl, b);
        if (wi.fail_reason) snprintf(b, sizeof b, "Ostatnia próba nieudana (powód %d) – sprawdź hasło i zasięg.", wi.fail_reason);
        else b[0] = 0;
        lv_label_set_text(ip_lbl, b);
    } else {
        lv_label_set_text(status_lbl, "Rozłączono");
        lv_label_set_text(ip_lbl, "Wybierz sieć z listy po wyszukaniu.");
    }

    // odśwież listę po zakończeniu skanowania
    uint32_t sig = wi.scanning ? 1u : 2u;
    if (!wi.scanning && s_scan_requested) { s_scan_requested = false; rebuild_list(); }
    else if (sig != s_sig) { s_sig = sig; if (!wi.scanning) rebuild_list(); }
    lv_label_set_text(ui_button_label(scan_btn), wi.scanning ? "Szukam…" : LV_SYMBOL_REFRESH "  Szukaj sieci");
}

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "gcode_stream.h"
#include "sdcard.h"
#include "ui_common.h"
#include "wifi_mgr.h"

#define MAX_FILES 64

static sd_file_t *files;          // w pamięci stałej, żeby nie obciążać stosu LVGL
static int nfiles;
static lv_obj_t *list, *info_lbl, *modal;
static char chosen[SD_PATH_MAX];  // wybrany plik: ścieżka względem karty
static char cwd[SD_PATH_MAX];     // bieżący katalog względem karty ("" = główny)
static lv_obj_t *path_lbl;
static grbl_status_t cur;
static uint32_t s_sig;

static void refresh_list(void);

static void close_modal(void)
{
    if (modal) { lv_obj_del(modal); modal = NULL; }
}

static void cancel_cb(lv_event_t *e) { close_modal(); }

static void start_cb(lv_event_t *e)
{
    const char *why = NULL;
    close_modal();
    if (gcode_stream_start(chosen, &why)) ui_goto(SCR_RUN);
    else ui_toast("%s", why ? why : "Nie można uruchomić");
}

// Ścieżka wpisu `name` w bieżącym katalogu; false, gdy nie mieści się w SD_PATH_MAX.
static bool join_path(char *out, size_t n, const char *name)
{
    int w = cwd[0] ? snprintf(out, n, "%s/%s", cwd, name) : snprintf(out, n, "%s", name);
    return w > 0 && (size_t)w < n;
}

static void go_up(void)
{
    char *slash = strrchr(cwd, '/');
    if (slash) *slash = 0; else cwd[0] = 0;
}

static void entry_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (modal) return;
    if (i == -1) { go_up(); refresh_list(); return; }
    if (i < 0 || i >= nfiles) return;
    if (files[i].is_dir) {
        char p[SD_PATH_MAX];
        if (!join_path(p, sizeof p, files[i].name)) { ui_toast("Zbyt długa ścieżka"); return; }
        strcpy(cwd, p);
        refresh_list();
        return;
    }
    if (!join_path(chosen, sizeof chosen, files[i].name)) { ui_toast("Zbyt długa ścieżka"); return; }

    modal = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(modal);
    lv_obj_set_size(modal, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(modal, LV_OPA_70, 0);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);      // przechwytuje dotyk
    lv_obj_t *box = ui_card(modal);
    lv_obj_set_width(box, 420);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, C_CARD, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, C_LINE, 0);
    lv_obj_set_style_pad_row(box, 14, 0);
    ui_label(box, "Uruchomić plik?", ui_f24, C_TEXT);
    lv_obj_t *n = ui_label(box, chosen, ui_f20, C_AMBER);
    lv_obj_set_width(n, LV_PCT(100));
    lv_label_set_long_mode(n, LV_LABEL_LONG_WRAP);
    lv_obj_t *w = ui_label(box, "Upewnij się, że materiał jest położony, ognisko ustawione, a laser skierowany na stół. "
                                "Pozycja zerowa to bieżąca pozycja robocza.", ui_f16, C_MUTED);
    lv_obj_set_width(w, LV_PCT(100));
    lv_label_set_long_mode(w, LV_LABEL_LONG_WRAP);
    lv_obj_t *row = ui_row(box, 76);
    lv_obj_t *c = ui_button(row, "Anuluj", ui_f20, cancel_cb, NULL);
    lv_obj_set_flex_grow(c, 1); lv_obj_set_height(c, 76);
    lv_obj_t *s = ui_button(row, LV_SYMBOL_PLAY "  Start", ui_f20, start_cb, NULL);
    lv_obj_set_flex_grow(s, 1); lv_obj_set_height(s, 76);
    ui_button_accent(s);
}

static void rescan_cb(lv_event_t *e)
{
    if (!sdcard_mounted() && !sdcard_mount()) ui_toast("Nie wykryto karty SD");
    s_sig = sdcard_generation();
    refresh_list();
}

static void size_text(char *b, size_t n, uint32_t sz)
{
    if (sz < 1024) snprintf(b, n, "%u B", (unsigned)sz);
    else if (sz < 1024 * 1024) snprintf(b, n, "%.1f kB", sz / 1024.0);
    else snprintf(b, n, "%.1f MB", sz / 1048576.0);
}

static void add_row(const char *icon, const char *name, lv_color_t name_col, const char *right_text, int idx)
{
    lv_obj_t *r = ui_button(list, NULL, ui_f20, entry_cb, (void *)(intptr_t)idx);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, 76);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_hor(r, 18, 0);
    lv_obj_set_style_pad_column(r, 12, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, C_DIV, 0);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_label(r, icon, ui_f20, C_AMBER);
    lv_obj_t *n = ui_label(r, name, ui_f20, name_col);
    lv_obj_set_flex_grow(n, 1);
    lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
    if (right_text) ui_label(r, right_text, ui_f16, C_MUTED);
}

static void refresh_list(void)
{
    lv_obj_clean(list);
    if (!files) files = heap_caps_malloc(sizeof(sd_file_t) * MAX_FILES, MALLOC_CAP_DEFAULT);

    if (!sdcard_mounted()) {
        cwd[0] = 0;
        nfiles = 0;
        lv_label_set_text(path_lbl, "");
        lv_obj_t *l = ui_label(list, "Brak karty SD.\nWłóż kartę - zostanie wykryta automatycznie.", ui_f20, C_MUTED);
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_pad_all(l, 24, 0);
        return;
    }

    nfiles = files ? sdcard_list(cwd, files, MAX_FILES) : 0;
    char p[SD_PATH_MAX + 8];
    snprintf(p, sizeof p, "/%s", cwd);
    lv_label_set_text(path_lbl, p);

    if (cwd[0]) add_row(LV_SYMBOL_LEFT, "..  (katalog wyżej)", C_MUTED, NULL, -1);
    if (nfiles == 0) {
        lv_obj_t *l = ui_label(list, "Brak podkatalogów ani plików G-code (.nc .gcode .gc .ngc .tap .txt) w tym katalogu.", ui_f20, C_MUTED);
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_pad_all(l, 24, 0);
        return;
    }
    for (int i = 0; i < nfiles; i++) {
        if (files[i].is_dir) {
            add_row(LV_SYMBOL_DIRECTORY, files[i].name, C_TEXT, LV_SYMBOL_RIGHT, i);
        } else {
            char sz[24];
            size_text(sz, sizeof sz, files[i].size);
            add_row(LV_SYMBOL_FILE, files[i].name, C_TEXT, sz, i);
        }
    }
}

lv_obj_t *ui_files_create(void)
{
    lv_obj_t *scr = ui_new_screen();
    lv_obj_t *right;
    ui_header(scr, "Karta SD", SCR_MAIN, &right);
    lv_obj_t *rb = ui_button(right, LV_SYMBOL_REFRESH, ui_f24, rescan_cb, NULL);
    lv_obj_set_size(rb, 64, 64);

    path_lbl = ui_label(scr, "", ui_f16, C_MUTED);
    lv_obj_set_width(path_lbl, LV_PCT(100));
    lv_label_set_long_mode(path_lbl, LV_LABEL_LONG_DOT);

    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_scroll_dir(card, LV_DIR_VER);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    list = card;

    info_lbl = ui_label(scr, "", ui_f16, C_MUTED);
    lv_obj_set_width(info_lbl, LV_PCT(100));
    lv_label_set_long_mode(info_lbl, LV_LABEL_LONG_WRAP);

    modal = NULL;
    cwd[0] = 0;
    s_sig = sdcard_generation();     // montowaniem zajmuje się zadanie nadzoru (sdcard_init)
    refresh_list();
    return scr;
}

void ui_files_update(const grbl_status_t *st)
{
    cur = *st;
    wifi_info_t wi;
    wifi_mgr_info(&wi);
    char b[96];
    if (wi.connected) snprintf(b, sizeof b, "Wgrywanie z komputera: otwórz http://%s/ w przeglądarce", wi.ip);
    else snprintf(b, sizeof b, "Wgrywanie plików przez Wi-Fi: połącz panel z siecią (Ustawienia › Panel › Wi-Fi).");
    lv_label_set_text(info_lbl, b);

    // wyjęcie / włożenie (także zamiana karty) - wracamy do katalogu głównego nowej karty
    uint32_t sig = sdcard_generation();
    if (sig != s_sig) {
        s_sig = sig;
        cwd[0] = 0;
        close_modal();               // okno "Uruchomić plik?" dotyczyło poprzedniej karty
        ui_toast("%s", sdcard_mounted() ? "Wykryto kartę SD" : "Karta SD wyjęta");
        refresh_list();
    }
}

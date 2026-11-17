#include <stdio.h>

#include "ui_common.h"

static lv_obj_t *pill, *code_lbl, *title_lbl, *text_lbl;
static grbl_status_t cur;

static const struct { int code; const char *title; const char *text; } ALARMS[] = {
    {1,  "Zadziałała krańcówka", "Ruch zatrzymany natychmiast. Pozycja maszyny mogła zostać utracona – przed dalszą pracą wykonaj bazowanie."},
    {2,  "Przekroczona granica ruchu", "Zadany ruch wychodzi poza obszar roboczy (granice miękkie). Pozycja jest zachowana – odblokuj i cofnij się."},
    {3,  "Przerwano w trakcie ruchu", "Reset w czasie pracy. Pozycja mogła zostać utracona – wykonaj bazowanie."},
    {4,  "Błąd sondy", "Sonda nie była w oczekiwanym stanie na początku cyklu."},
    {5,  "Sonda nie zetknęła się", "Sonda nie zadziałała na zadanej drodze."},
    {6,  "Bazowanie przerwane", "Reset w trakcie cyklu bazowania."},
    {7,  "Bazowanie: otwarta osłona", "Osłona została otwarta w trakcie bazowania."},
    {8,  "Bazowanie: nie zwolniono krańcówki", "Nie udało się odjechać od krańcówki po bazowaniu. Sprawdź przewody i polaryzację krańcówek."},
    {9,  "Bazowanie: nie znaleziono krańcówki", "Krańcówka nie zadziałała na drodze szukania. Sprawdź przewody, polaryzację ($5) i kierunek bazowania ($23)."},
    {10, "Zatrzymanie awaryjne", "Aktywny sygnał E-stop. Zwolnij przycisk awaryjny i zresetuj sterownik."},
    {11, "Wymagane bazowanie", "Przed pracą trzeba wykonać bazowanie."},
    {12, "Krańcówka aktywna", "Krańcówka jest zwarta – odsuń oś ręcznie albo odblokuj i wyjedź jogiem."},
    {13, "Ochrona sondy", "Sonda jest aktywna."},
    {14, "Błąd sterowania wrzecionem/laserem", "Sprawdź ustawienia lasera ($30–$33)."},
    {16, "Błąd autotestu", "Sterownik nie przeszedł autotestu przy starcie."},
};

static void home_cb(lv_event_t *e)   { grbl_send_line("$H"); }
static void unlock_cb(lv_event_t *e) { grbl_send_line("$X"); }
static void reset_cb(lv_event_t *e)  { grbl_send_realtime(0x18); }

static lv_obj_t *action(lv_obj_t *parent, const char *name, const char *code, int h, lv_event_cb_t cb, bool accent)
{
    lv_obj_t *b = ui_button(parent, name, ui_f24, cb, NULL);
    lv_obj_set_height(b, h);
    ui_label(b, code, ui_f16, accent ? C_ON_AMBER : C_MUTED);
    if (accent) ui_button_accent(b);
    return b;
}

lv_obj_t *ui_alarm_create(void)
{
    lv_obj_t *scr = ui_new_screen();

    lv_obj_t *hdr = ui_row(scr, 64);
    pill = ui_pill(hdr);
    lv_obj_t *sub = ui_label(hdr, "Laser 2D", ui_f16, C_MUTED);
    lv_obj_set_flex_grow(sub, 1);

    lv_obj_t *card = ui_card(scr);
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1F1214), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x6B2226), 0);
    lv_obj_set_style_pad_hor(card, 24, 0);
    lv_obj_set_style_pad_row(card, 16, 0);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *icon = ui_label(card, LV_SYMBOL_WARNING, &lv_font_montserrat_32, C_RED_TXT);
    (void)icon;
    code_lbl = ui_label(card, "ALARM", ui_f20, C_RED_TXT);
    title_lbl = ui_label(card, "Alarm", ui_f32, C_TEXT);
    lv_obj_set_width(title_lbl, LV_PCT(100));
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_WRAP);
    text_lbl = ui_label(card, "", ui_f20, lv_color_hex(0xD5C6C7));
    lv_obj_set_width(text_lbl, LV_PCT(100));
    lv_label_set_long_mode(text_lbl, LV_LABEL_LONG_WRAP);

    lv_obj_t *home = action(scr, "Bazuj", "$H", 104, home_cb, true);
    lv_obj_set_width(home, LV_PCT(100));
    lv_obj_t *row = ui_row(scr, 88);
    lv_obj_t *u = action(row, "Odblokuj", "$X", 88, unlock_cb, false);
    lv_obj_set_flex_grow(u, 1);
    lv_obj_t *r = action(row, "Reset", "Ctrl-X", 88, reset_cb, false);
    lv_obj_set_flex_grow(r, 1);
    return scr;
}

void ui_alarm_update(const grbl_status_t *st)
{
    cur = *st;
    ui_pill_set(pill, st);
    char b[32];
    snprintf(b, sizeof b, "ALARM:%d", st->alarm);
    lv_label_set_text(code_lbl, st->alarm ? b : "ALARM");

    const char *title = "Alarm", *text = "Sterownik zgłosił alarm. Odblokuj albo wykonaj bazowanie.";
    for (unsigned i = 0; i < sizeof ALARMS / sizeof ALARMS[0]; i++)
        if (ALARMS[i].code == st->alarm) { title = ALARMS[i].title; text = ALARMS[i].text; }
    lv_label_set_text(title_lbl, title);
    lv_label_set_text(text_lbl, text);
}

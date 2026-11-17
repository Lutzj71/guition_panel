#pragma once

#include "ui.h"

// Paleta z makiety (ciemny motyw, akcent bursztynowy)
#define C_BG        lv_color_hex(0x0F1216)
#define C_CARD      lv_color_hex(0x171B21)
#define C_BTN       lv_color_hex(0x1C2128)
#define C_BTN2      lv_color_hex(0x222831)
#define C_BTN_PRESS lv_color_hex(0x2C343E)
#define C_LINE      lv_color_hex(0x2A313A)
#define C_DIV       lv_color_hex(0x252B33)
#define C_TEXT      lv_color_hex(0xE8EAED)
#define C_MUTED     lv_color_hex(0x9AA3AD)
#define C_DIM       lv_color_hex(0x5C6670)
#define C_AMBER     lv_color_hex(0xFFB020)
#define C_ON_AMBER  lv_color_hex(0x1A1300)
#define C_RED       lv_color_hex(0xC62F35)
#define C_RED_TXT   lv_color_hex(0xFF8A8F)
#define C_GREEN     lv_color_hex(0x5BE39A)
#define C_BLUE      lv_color_hex(0x7CC0FF)

// Czcionki: Montserrat z polskimi znakami (TinyTTF z pliku osadzonego w firmware), ikony LV_SYMBOL_* z czcionki wbudowanej.
extern lv_font_t *ui_f16, *ui_f20, *ui_f24, *ui_f32, *ui_f56, *ui_f68;
void ui_fonts_init(void);

lv_obj_t *ui_new_screen(void);                                   // tło, kolumna z odstępem 12, padding 16
lv_obj_t *ui_row(lv_obj_t *parent, int height);                  // przezroczysty rząd (height <= 0: wg zawartości)
lv_obj_t *ui_col(lv_obj_t *parent);                              // przezroczysta kolumna
lv_obj_t *ui_card(lv_obj_t *parent);                             // karta #171B21, promień 16, padding 18/22
lv_obj_t *ui_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color);
lv_obj_t *ui_button(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_event_cb_t cb, void *user);
void ui_button_accent(lv_obj_t *btn);                            // bursztynowe tło, ciemny tekst
void ui_button_danger(lv_obj_t *btn);                            // czerwone tło
void ui_button_selected(lv_obj_t *btn, bool selected);           // wybrana opcja (krok, posuw, zakładka)
lv_obj_t *ui_button_label(lv_obj_t *btn);
lv_obj_t *ui_header(lv_obj_t *screen, const char *title, ui_scr_t back_to, lv_obj_t **right);  // rząd 64 px: wstecz + tytuł
lv_obj_t *ui_pill(lv_obj_t *parent);                             // plakietka stanu
void ui_pill_set(lv_obj_t *pill, const grbl_status_t *st);
lv_obj_t *ui_dot(lv_obj_t *parent, int d, lv_color_t color);
void ui_fmt_mm(char *buf, size_t n, float v);                    // "125.400"
const char *ui_state_text(const grbl_status_t *st);              // "IDLE", "RUN", "BRAK ŁĄCZA", ...

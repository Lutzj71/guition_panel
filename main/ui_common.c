#include "ui_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Montserrat Medium (OFL) osadzony przez EMBED_FILES w main/CMakeLists.txt
extern const uint8_t ttf_start[] asm("_binary_Montserrat_Medium_ttf_start");
extern const uint8_t ttf_end[] asm("_binary_Montserrat_Medium_ttf_end");

lv_font_t *ui_f16, *ui_f20, *ui_f24, *ui_f32, *ui_f56, *ui_f68;

// cache_glyphs: ile glifów trzymać w pamięci podręcznej. LVGL 8.4 przyjmuje rozmiar w BAJTACH i
// zakłada średnio px*px bajtów na glif (tablica haszująca = bajty / px^2).
static lv_font_t *mk_font(int px, size_t cache_glyphs)
{
    lv_font_t *f = lv_tiny_ttf_create_data_ex(ttf_start, ttf_end - ttf_start, px, cache_glyphs * px * px);
    if (f) f->fallback = &lv_font_montserrat_20;   // ikony LV_SYMBOL_* i znaki spoza TTF
    return f;
}

void ui_fonts_init(void)
{
    ui_f16 = mk_font(16, 256);
    ui_f20 = mk_font(20, 256);
    ui_f24 = mk_font(24, 256);
    ui_f32 = mk_font(32, 128);
    ui_f56 = mk_font(56, 32);
    ui_f68 = mk_font(68, 32);
    // awaryjnie: gdyby TinyTTF nie powstał, użyj czcionek wbudowanych (bez polskich znaków)
    if (!ui_f16) ui_f16 = (lv_font_t *)&lv_font_montserrat_16;
    if (!ui_f20) ui_f20 = (lv_font_t *)&lv_font_montserrat_20;
    if (!ui_f24) ui_f24 = (lv_font_t *)&lv_font_montserrat_20;
    if (!ui_f32) ui_f32 = (lv_font_t *)&lv_font_montserrat_32;
    if (!ui_f56) ui_f56 = (lv_font_t *)&lv_font_montserrat_32;
    if (!ui_f68) ui_f68 = (lv_font_t *)&lv_font_montserrat_32;
}

// ---------------------------------------------------------------- układ

lv_obj_t *ui_new_screen(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_remove_style_all(s);
    lv_obj_set_size(s, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s, C_BG, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s, C_TEXT, 0);
    lv_obj_set_style_text_font(s, ui_f20, 0);
    lv_obj_set_style_pad_all(s, 16, 0);
    lv_obj_set_style_pad_row(s, 12, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
    return s;
}

lv_obj_t *ui_row(lv_obj_t *parent, int height)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_width(r, LV_PCT(100));
    if (height > 0) lv_obj_set_height(r, height); else lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(r, 12, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return r;
}

lv_obj_t *ui_col(lv_obj_t *parent)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_width(c, LV_SIZE_CONTENT);
    lv_obj_set_height(c, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(c, 3, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    return c;
}

lv_obj_t *ui_card(lv_obj_t *parent)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_width(c, LV_PCT(100));
    lv_obj_set_height(c, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(c, C_CARD, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 16, 0);
    lv_obj_set_style_pad_hor(c, 22, 0);
    lv_obj_set_style_pad_ver(c, 18, 0);
    lv_obj_set_style_pad_row(c, 6, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    return c;
}

lv_obj_t *ui_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

lv_obj_t *ui_dot(lv_obj_t *parent, int d, lv_color_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, color, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

// ---------------------------------------------------------------- przyciski

lv_obj_t *ui_button(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_event_cb_t cb, void *user)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_style_bg_color(b, C_BTN, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, C_BTN_PRESS, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, C_LINE, 0);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_text_color(b, C_TEXT, 0);
    lv_obj_set_style_text_font(b, font, 0);
    lv_obj_set_style_pad_all(b, 6, 0);
    lv_obj_set_style_pad_row(b, 4, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (text) {
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, text);
        lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    }
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    return b;
}

lv_obj_t *ui_button_label(lv_obj_t *btn)
{
    return lv_obj_get_child(btn, -1);
}

void ui_button_accent(lv_obj_t *b)
{
    lv_obj_set_style_bg_color(b, C_AMBER, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0xE09A10), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_text_color(b, C_ON_AMBER, 0);
}

void ui_button_danger(lv_obj_t *b)
{
    lv_obj_set_style_bg_color(b, C_RED, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0xA32529), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_text_color(b, lv_color_hex(0xFFFFFF), 0);
}

void ui_button_selected(lv_obj_t *b, bool sel)
{
    lv_obj_set_style_bg_color(b, sel ? C_AMBER : C_BTN, 0);
    lv_obj_set_style_border_color(b, sel ? C_AMBER : C_LINE, 0);
    lv_obj_set_style_text_color(b, sel ? C_ON_AMBER : C_TEXT, 0);
}

static void back_cb(lv_event_t *e)
{
    ui_goto((ui_scr_t)(intptr_t)lv_event_get_user_data(e));
}

lv_obj_t *ui_header(lv_obj_t *screen, const char *title, ui_scr_t back_to, lv_obj_t **right)
{
    lv_obj_t *h = ui_row(screen, 64);
    lv_obj_set_style_pad_column(h, 14, 0);
    lv_obj_t *b = ui_button(h, LV_SYMBOL_LEFT, ui_f24, back_cb, (void *)(intptr_t)back_to);
    lv_obj_set_size(b, 64, 64);
    lv_obj_t *t = ui_label(h, title, ui_f24, C_TEXT);
    lv_obj_set_flex_grow(t, 1);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    if (right) {
        lv_obj_t *r = ui_row(h, 0);
        lv_obj_set_width(r, LV_SIZE_CONTENT);
        *right = r;
    }
    return h;
}

// ---------------------------------------------------------------- stan

const char *ui_state_text(const grbl_status_t *st)
{
    if (st->stale) return "BRAK ŁĄCZA";
    switch (st->state_id) {
    case GRBL_ST_IDLE:  return "IDLE";
    case GRBL_ST_RUN:   return "RUN";
    case GRBL_ST_HOLD:  return "HOLD";
    case GRBL_ST_JOG:   return "JOG";
    case GRBL_ST_ALARM: return "ALARM";
    case GRBL_ST_HOME:  return "HOME";
    case GRBL_ST_DOOR:  return "DOOR";
    case GRBL_ST_CHECK: return "CHECK";
    case GRBL_ST_SLEEP: return "SLEEP";
    default:            return "...";
    }
}

lv_obj_t *ui_pill(lv_obj_t *parent)
{
    lv_obj_t *p = lv_label_create(parent);
    lv_label_set_text(p, "...");
    lv_obj_set_style_text_font(p, ui_f20, 0);
    lv_obj_set_style_pad_hor(p, 18, 0);
    lv_obj_set_style_pad_ver(p, 12, 0);
    lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_text_letter_space(p, 1, 0);
    return p;
}

void ui_pill_set(lv_obj_t *pill, const grbl_status_t *st)
{
    lv_color_t bg = lv_color_hex(0x16324F), fg = C_BLUE;   // IDLE
    if (st->stale)                             { bg = C_BTN;                    fg = C_MUTED; }
    else if (st->state_id == GRBL_ST_ALARM)    { bg = lv_color_hex(0x4A1517);   fg = C_RED_TXT; }
    else if (st->state_id == GRBL_ST_RUN || st->state_id == GRBL_ST_JOG || st->state_id == GRBL_ST_HOME)
                                               { bg = lv_color_hex(0x123A26);   fg = C_GREEN; }
    else if (st->state_id == GRBL_ST_HOLD || st->state_id == GRBL_ST_DOOR)
                                               { bg = lv_color_hex(0x3A2C0F);   fg = C_AMBER; }
    lv_label_set_text(pill, ui_state_text(st));
    lv_obj_set_style_bg_color(pill, bg, 0);
    lv_obj_set_style_text_color(pill, fg, 0);
}

void ui_fmt_mm(char *buf, size_t n, float v)
{
    snprintf(buf, n, "%.3f", (double)v);
}

// ---------------------------------------------------------------- komunikat

static lv_obj_t *s_toast;
static lv_timer_t *s_toast_timer;

static void toast_hide(lv_timer_t *t)
{
    if (s_toast) lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    s_toast_timer = NULL;
}

void ui_toast(const char *fmt, ...)
{
    char buf[120];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    if (!s_toast) {
        s_toast = lv_label_create(lv_layer_top());
        lv_obj_set_style_bg_color(s_toast, lv_color_hex(0x2C343E), 0);
        lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(s_toast, C_TEXT, 0);
        lv_obj_set_style_text_font(s_toast, ui_f20, 0);
        lv_obj_set_style_border_width(s_toast, 1, 0);
        lv_obj_set_style_border_color(s_toast, C_LINE, 0);
        lv_obj_set_style_radius(s_toast, 12, 0);
        lv_obj_set_style_pad_hor(s_toast, 18, 0);
        lv_obj_set_style_pad_ver(s_toast, 12, 0);
        lv_obj_set_width(s_toast, 440);
        lv_obj_set_style_text_align(s_toast, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_label_set_text(s_toast, buf);
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -130);
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_toast);
    if (s_toast_timer) lv_timer_reset(s_toast_timer);
    else {
        s_toast_timer = lv_timer_create(toast_hide, 2600, NULL);
        lv_timer_set_repeat_count(s_toast_timer, 1);
    }
}

bool ui_can_command(const grbl_status_t *st)
{
    if (st->stale) { ui_toast("Brak łącza z maszyną"); return false; }
    if (st->mpg == 0) { ui_toast("Sterowanie ma komputer – dotknij „Komputer steruje”, aby przejąć"); return false; }
    return true;
}

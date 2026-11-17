#include <stdio.h>
#include <string.h>

#include "gcode_stream.h"
#include "ui_common.h"

static lv_obj_t *pill, *sub, *chip_dot, *chip_lbl;
static lv_obj_t *x_lbl, *y_lbl, *feed_lbl, *pwr_lbl, *laser_dot, *laser_lbl;
static lv_obj_t *prog_wrap, *prog_bar, *prog_lbl;
static lv_obj_t *ov_feed_lbl, *ov_pwr_lbl, *pause_btn, *pause_lbl, *stop_btn, *msg_lbl;
static grbl_status_t cur;

// korekty czasu rzeczywistego grblHAL
enum { OV_FEED_RESET = 0x90, OV_FEED_UP = 0x91, OV_FEED_DOWN = 0x92,
       OV_PWR_RESET = 0x99, OV_PWR_UP = 0x9A, OV_PWR_DOWN = 0x9B };

static void ov_cb(lv_event_t *e)
{
    grbl_send_realtime((char)(intptr_t)lv_event_get_user_data(e));
}

static void pause_cb(lv_event_t *e)
{
    gs_status_t gs;
    gcode_stream_get(&gs);
    bool held = cur.state_id == GRBL_ST_HOLD || cur.state_id == GRBL_ST_DOOR;
    if (gs.state == GS_RUNNING) gcode_stream_pause();
    else if (gs.state == GS_PAUSED) gcode_stream_resume();
    else if (gs.state == GS_DONE || gs.state == GS_ABORTED || gs.state == GS_ERROR) { gcode_stream_dismiss(); ui_goto(SCR_MAIN); }
    else grbl_send_realtime(held ? '~' : '!');       // zadanie z komputera
}

static void stop_cb(lv_event_t *e)
{
    gs_status_t gs;
    gcode_stream_get(&gs);
    if (gs.state == GS_RUNNING || gs.state == GS_PAUSED || gs.state == GS_ERROR) gcode_stream_stop();
    else if (gs.state == GS_DONE || gs.state == GS_ABORTED) { gcode_stream_dismiss(); ui_goto(SCR_MAIN); }
    else { grbl_send_realtime('!'); grbl_send_realtime((char)0x18); }
}

static lv_obj_t *stepper(lv_obj_t *parent, const char *caption, lv_color_t vcol, int down, int up, int reset, lv_obj_t **val)
{
    lv_obj_t *card = ui_card(parent);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 12, 0);
    lv_obj_t *m = ui_button(card, LV_SYMBOL_MINUS, ui_f32, ov_cb, (void *)(intptr_t)down);
    lv_obj_set_size(m, 76, 76);
    lv_obj_set_style_bg_color(m, C_BTN2, 0);
    lv_obj_t *mid = ui_button(card, NULL, ui_f20, ov_cb, (void *)(intptr_t)reset);   // dotknięcie = 100 %
    lv_obj_set_flex_grow(mid, 1);
    lv_obj_set_height(mid, 76);
    lv_obj_set_style_bg_opa(mid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mid, 0, 0);
    ui_label(mid, caption, ui_f16, C_MUTED);
    *val = ui_label(mid, "100%", ui_f32, vcol);
    lv_obj_t *p = ui_button(card, LV_SYMBOL_PLUS, ui_f32, ov_cb, (void *)(intptr_t)up);
    lv_obj_set_size(p, 76, 76);
    lv_obj_set_style_bg_color(p, C_BTN2, 0);
    return card;
}

lv_obj_t *ui_run_create(void)
{
    lv_obj_t *scr = ui_new_screen();

    lv_obj_t *hdr = ui_row(scr, 64);
    pill = ui_pill(hdr);
    sub = ui_label(hdr, "Zadanie z komputera", ui_f16, C_MUTED);
    lv_obj_set_flex_grow(sub, 1);
    lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
    lv_obj_t *chip = ui_row(hdr, 48);
    lv_obj_set_width(chip, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(chip, 16, 0);
    lv_obj_set_style_bg_color(chip, C_BTN, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_style_border_color(chip, C_LINE, 0);
    lv_obj_set_style_radius(chip, 14, 0);
    lv_obj_set_style_pad_column(chip, 10, 0);
    chip_dot = ui_dot(chip, 11, C_BLUE);
    chip_lbl = ui_label(chip, "PC steruje", ui_f16, C_TEXT);

    lv_obj_t *card = ui_card(scr);
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *pos = ui_col(card);
    lv_obj_t *rx = ui_row(pos, 0);
    ui_label(rx, "X", ui_f32, C_AMBER);
    x_lbl = ui_label(rx, "0.000", ui_f56, C_TEXT);
    lv_obj_t *ry = ui_row(pos, 0);
    ui_label(ry, "Y", ui_f32, C_AMBER);
    y_lbl = ui_label(ry, "0.000", ui_f56, C_TEXT);

    prog_wrap = ui_col(card);
    lv_obj_set_width(prog_wrap, LV_PCT(100));
    lv_obj_set_style_pad_row(prog_wrap, 6, 0);
    prog_lbl = ui_label(prog_wrap, "", ui_f16, C_MUTED);
    prog_bar = lv_bar_create(prog_wrap);
    lv_obj_set_size(prog_bar, LV_PCT(100), 12);
    lv_bar_set_range(prog_bar, 0, 1000);
    lv_obj_set_style_bg_color(prog_bar, C_BTN2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(prog_bar, C_AMBER, LV_PART_INDICATOR);
    lv_obj_add_flag(prog_wrap, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *vals = ui_row(card, 0);
    lv_obj_t *c1 = ui_col(vals); lv_obj_set_flex_grow(c1, 1);
    ui_label(c1, "POSUW · mm/min", ui_f16, C_MUTED);
    feed_lbl = ui_label(c1, "0", ui_f32, C_TEXT);
    lv_obj_t *c2 = ui_col(vals); lv_obj_set_flex_grow(c2, 1);
    ui_label(c2, "MOC LASERA", ui_f16, C_MUTED);
    pwr_lbl = ui_label(c2, "S0", ui_f32, C_TEXT);

    lv_obj_t *lrow = ui_row(card, 0);
    laser_dot = ui_dot(lrow, 14, C_DIM);
    laser_lbl = ui_label(lrow, "Laser wyłączony", ui_f20, C_MUTED);

    msg_lbl = ui_label(scr, "", ui_f16, C_RED_TXT);
    lv_obj_set_width(msg_lbl, LV_PCT(100));
    lv_label_set_long_mode(msg_lbl, LV_LABEL_LONG_WRAP);

    stepper(scr, "POSUW", C_TEXT, OV_FEED_DOWN, OV_FEED_UP, OV_FEED_RESET, &ov_feed_lbl);
    stepper(scr, "MOC LASERA", C_AMBER, OV_PWR_DOWN, OV_PWR_UP, OV_PWR_RESET, &ov_pwr_lbl);

    lv_obj_t *act = ui_row(scr, 112);
    lv_obj_set_style_pad_column(act, 12, 0);
    pause_btn = ui_button(act, NULL, ui_f24, pause_cb, NULL);
    lv_obj_set_flex_grow(pause_btn, 14);
    lv_obj_set_height(pause_btn, 112);
    ui_button_accent(pause_btn);
    pause_lbl = lv_label_create(pause_btn);
    lv_label_set_text(pause_lbl, LV_SYMBOL_PAUSE "  PAUZA");
    stop_btn = ui_button(act, LV_SYMBOL_STOP "  STOP", ui_f24, stop_cb, NULL);
    lv_obj_set_flex_grow(stop_btn, 10);
    lv_obj_set_height(stop_btn, 112);
    ui_button_danger(stop_btn);
    return scr;
}

void ui_run_update(const grbl_status_t *st)
{
    cur = *st;
    gs_status_t gs;
    gcode_stream_get(&gs);
    char b[192];

    ui_pill_set(pill, st);
    bool job = gs.state != GS_IDLE;
    lv_label_set_text(sub, job ? gs.name : "Zadanie z komputera");
    lv_label_set_text(chip_lbl, st->mpg == 1 ? "Panel steruje" : st->mpg == 0 ? "PC steruje" : "Sterowanie: ?");
    lv_obj_set_style_bg_color(chip_dot, st->mpg == 1 ? C_AMBER : C_BLUE, 0);

    ui_fmt_mm(b, sizeof b, st->wx); lv_label_set_text(x_lbl, b);
    ui_fmt_mm(b, sizeof b, st->wy); lv_label_set_text(y_lbl, b);
    snprintf(b, sizeof b, "%d", (int)st->feed); lv_label_set_text(feed_lbl, b);
    snprintf(b, sizeof b, "S%d", (int)st->spindle); lv_label_set_text(pwr_lbl, b);
    lv_obj_set_style_bg_color(laser_dot, st->laser_on ? C_AMBER : C_DIM, 0);
    lv_label_set_text(laser_lbl, st->laser_on ? "Laser włączony" : "Laser wyłączony");
    lv_obj_set_style_text_color(laser_lbl, st->laser_on ? C_AMBER : C_MUTED, 0);
    snprintf(b, sizeof b, "%d%%", st->ov_feed); lv_label_set_text(ov_feed_lbl, b);
    snprintf(b, sizeof b, "%d%%", st->ov_spindle); lv_label_set_text(ov_pwr_lbl, b);

    // postęp zadania z karty SD
    if (job) {
        lv_obj_clear_flag(prog_wrap, LV_OBJ_FLAG_HIDDEN);
        int permille = gs.bytes_total ? (int)((uint64_t)gs.bytes_done * 1000 / gs.bytes_total) : 0;
        if (gs.state == GS_DONE) permille = 1000;
        lv_bar_set_value(prog_bar, permille, LV_ANIM_OFF);
        const char *tag = gs.state == GS_PAUSED ? " · wstrzymane" : gs.state == GS_DONE ? " · zakończone" :
                          gs.state == GS_ABORTED ? " · przerwane" : gs.state == GS_ERROR ? " · BŁĄD" : "";
        snprintf(b, sizeof b, "%d.%d%% · linie %u/%u%s", permille / 10, permille % 10,
                 (unsigned)gs.lines_acked, (unsigned)gs.lines_sent, tag);
        lv_label_set_text(prog_lbl, b);
    } else {
        lv_obj_add_flag(prog_wrap, LV_OBJ_FLAG_HIDDEN);
    }

    if (gs.state == GS_ERROR) {
        if (gs.error) snprintf(b, sizeof b, "error:%d w linii: %s", gs.error, gs.error_line);
        else snprintf(b, sizeof b, "%s", gs.message);
        lv_label_set_text(msg_lbl, b);
    } else {
        lv_label_set_text(msg_lbl, "");
    }

    // przycisk pauzy / wznowienia / zamknięcia
    bool held = st->state_id == GRBL_ST_HOLD || st->state_id == GRBL_ST_DOOR;
    const char *t = LV_SYMBOL_PAUSE "  PAUZA";
    if (gs.state == GS_PAUSED || (gs.state == GS_IDLE && held)) t = LV_SYMBOL_PLAY "  WZNÓW";
    if (gs.state == GS_DONE || gs.state == GS_ABORTED || gs.state == GS_ERROR) t = LV_SYMBOL_OK "  ZAMKNIJ";
    lv_label_set_text(pause_lbl, t);
}

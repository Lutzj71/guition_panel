#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "esp_lvgl_port.h"
#include "grbl_link.h"

#define JOG_FEED 3000   // mm/min

static lv_obj_t *s_state, *s_x, *s_y, *s_msg;
static float s_step = 1.0f;

typedef enum { CMD_HOME, CMD_UNLOCK, CMD_HOLD, CMD_RESUME, CMD_RESET, CMD_JOG_XP, CMD_JOG_XM, CMD_JOG_YP, CMD_JOG_YM } cmd_t;

static void cmd_cb(lv_event_t *e)
{
    cmd_t c = (cmd_t)(intptr_t)lv_event_get_user_data(e);
    char buf[64];
    switch (c) {
    case CMD_HOME:   grbl_send_line("$H"); break;
    case CMD_UNLOCK: grbl_send_line("$X"); break;
    case CMD_HOLD:   grbl_send_realtime('!'); break;
    case CMD_RESUME: grbl_send_realtime('~'); break;
    case CMD_RESET:  grbl_send_realtime(0x18); break;
    case CMD_JOG_XP: snprintf(buf, sizeof buf, "$J=G91 G21 X%.3f F%d", s_step, JOG_FEED); grbl_send_line(buf); break;
    case CMD_JOG_XM: snprintf(buf, sizeof buf, "$J=G91 G21 X-%.3f F%d", s_step, JOG_FEED); grbl_send_line(buf); break;
    case CMD_JOG_YP: snprintf(buf, sizeof buf, "$J=G91 G21 Y%.3f F%d", s_step, JOG_FEED); grbl_send_line(buf); break;
    case CMD_JOG_YM: snprintf(buf, sizeof buf, "$J=G91 G21 Y-%.3f F%d", s_step, JOG_FEED); grbl_send_line(buf); break;
    }
}

static void step_cb(lv_event_t *e)
{
    static const float steps[] = {0.1f, 1.0f, 10.0f, 50.0f};
    lv_obj_t *m = lv_event_get_target(e);
    uint16_t id = lv_btnmatrix_get_selected_btn(m);
    if (id < 4) s_step = steps[id];
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *text, cmd_t cmd, int w, int h)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cmd_cb, LV_EVENT_CLICKED, (void *)(intptr_t)cmd);
    return b;
}

static void refresh_cb(lv_timer_t *t)
{
    grbl_status_t st;
    grbl_link_get_status(&st);
    char buf[48];

    if (st.stale) {
        lv_label_set_text(s_state, "BRAK LACZA");
        lv_obj_set_style_text_color(s_state, lv_palette_main(LV_PALETTE_GREY), 0);
        return;
    }
    lv_label_set_text(s_state, st.state);
    lv_color_t col = lv_palette_main(LV_PALETTE_GREEN);
    if (!strncmp(st.state, "Alarm", 5)) col = lv_palette_main(LV_PALETTE_RED);
    else if (!strncmp(st.state, "Hold", 4) || !strncmp(st.state, "Door", 4)) col = lv_palette_main(LV_PALETTE_ORANGE);
    else if (!strncmp(st.state, "Run", 3) || !strncmp(st.state, "Jog", 3) || !strncmp(st.state, "Home", 4)) col = lv_palette_main(LV_PALETTE_BLUE);
    lv_obj_set_style_text_color(s_state, col, 0);

    snprintf(buf, sizeof buf, "X %9.3f", st.x);
    lv_label_set_text(s_x, buf);
    snprintf(buf, sizeof buf, "Y %9.3f", st.y);
    lv_label_set_text(s_y, buf);
    lv_label_set_text(s_msg, st.last_msg);
}

void ui_create(lv_disp_t *disp)
{
    lvgl_port_lock(0);
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    lv_obj_set_style_pad_all(scr, 12, 0);

    s_state = lv_label_create(scr);
    lv_obj_set_style_text_font(s_state, &lv_font_montserrat_32, 0);
    lv_label_set_text(s_state, "...");
    lv_obj_align(s_state, LV_ALIGN_TOP_MID, 0, 8);

    s_x = lv_label_create(scr);
    lv_obj_set_style_text_font(s_x, &lv_font_montserrat_48, 0);
    lv_label_set_text(s_x, "X    0.000");
    lv_obj_align(s_x, LV_ALIGN_TOP_LEFT, 8, 70);

    s_y = lv_label_create(scr);
    lv_obj_set_style_text_font(s_y, &lv_font_montserrat_48, 0);
    lv_label_set_text(s_y, "Y    0.000");
    lv_obj_align(s_y, LV_ALIGN_TOP_LEFT, 8, 135);

    // Krok jogu
    static const char *step_map[] = {"0.1", "1", "10", "50", ""};
    lv_obj_t *steps = lv_btnmatrix_create(scr);
    lv_btnmatrix_set_map(steps, step_map);
    lv_btnmatrix_set_btn_ctrl_all(steps, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(steps, true);
    lv_btnmatrix_set_btn_ctrl(steps, 1, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_size(steps, 440, 64);
    lv_obj_align(steps, LV_ALIGN_TOP_MID, 0, 215);
    lv_obj_add_event_cb(steps, step_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Krzyżak jogu
    const int bw = 130, bh = 100;
    lv_obj_t *b;
    b = make_btn(scr, "Y+", CMD_JOG_YP, bw, bh); lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 300);
    b = make_btn(scr, "X-", CMD_JOG_XM, bw, bh); lv_obj_align(b, LV_ALIGN_TOP_MID, -(bw + 10), 410);
    b = make_btn(scr, "X+", CMD_JOG_XP, bw, bh); lv_obj_align(b, LV_ALIGN_TOP_MID, bw + 10, 410);
    b = make_btn(scr, "Y-", CMD_JOG_YM, bw, bh); lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 520);

    // Komendy
    b = make_btn(scr, "Home", CMD_HOME, 140, 70);   lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 0, -90);
    b = make_btn(scr, "Unlock", CMD_UNLOCK, 140, 70); lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -90);
    b = make_btn(scr, "Reset", CMD_RESET, 140, 70); lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, 0, -90);
    lv_obj_set_style_bg_color(b, lv_palette_main(LV_PALETTE_RED), 0);
    b = make_btn(scr, "Hold", CMD_HOLD, 210, 70);   lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    b = make_btn(scr, "Resume", CMD_RESUME, 210, 70); lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    s_msg = lv_label_create(scr);
    lv_label_set_text(s_msg, "");
    lv_obj_align(s_msg, LV_ALIGN_BOTTOM_MID, 0, -170);

    lv_timer_create(refresh_cb, 200, NULL);
    lvgl_port_unlock();
}

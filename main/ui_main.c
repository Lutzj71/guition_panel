#include <stdio.h>

#include "gcode_stream.h"
#include "ui_common.h"

static lv_obj_t *pill, *mpg_btn, *mpg_dot, *mpg_lbl;
static lv_obj_t *x_lbl, *y_lbl, *mach_lbl, *feed_lbl, *pwr_lbl, *laser_dot, *laser_lbl;

static grbl_status_t cur;

static void mpg_cb(lv_event_t *e)
{
    grbl_mpg_toggle();
    ui_toast("Przełączam sterowanie…");
}

static void home_cb(lv_event_t *e)
{
    if (!ui_can_command(&cur)) return;
    grbl_send_line("$H");
}

static void zero_cb(lv_event_t *e)
{
    if (!ui_can_command(&cur)) return;
    grbl_send_line("G10 L20 P0 X0 Y0");
}

static void nav_cb(lv_event_t *e)
{
    ui_goto((ui_scr_t)(intptr_t)lv_event_get_user_data(e));
}

static lv_obj_t *tile(lv_obj_t *grid, const char *icon, const char *text, lv_event_cb_t cb, void *user, int col, int span, int row)
{
    lv_obj_t *b = ui_button(grid, NULL, ui_f20, cb, user);
    lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, col, span, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_t *i = lv_label_create(b);
    lv_label_set_text(i, icon);
    lv_obj_set_style_text_font(i, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(i, C_AMBER, 0);
    lv_obj_t *t = lv_label_create(b);
    lv_label_set_text(t, text);
    lv_obj_set_style_text_font(t, ui_f20, 0);
    return b;
}

static lv_obj_t *small_card(lv_obj_t *row, const char *caption, lv_obj_t **value)
{
    lv_obj_t *c = ui_card(row);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_style_pad_ver(c, 14, 0);
    lv_obj_set_style_pad_hor(c, 18, 0);
    lv_obj_set_style_pad_row(c, 4, 0);
    ui_label(c, caption, ui_f16, C_MUTED);
    *value = ui_label(c, "0", ui_f32, C_TEXT);
    return c;
}

lv_obj_t *ui_main_create(void)
{
    lv_obj_t *scr = ui_new_screen();

    lv_obj_t *hdr = ui_row(scr, 64);
    pill = ui_pill(hdr);
    lv_obj_t *sub = ui_label(hdr, "Laser 2D", ui_f16, C_MUTED);
    lv_obj_set_flex_grow(sub, 1);
    mpg_btn = ui_button(hdr, NULL, ui_f16, mpg_cb, NULL);
    lv_obj_set_height(mpg_btn, 64);
    lv_obj_set_style_pad_hor(mpg_btn, 18, 0);
    lv_obj_set_flex_flow(mpg_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(mpg_btn, 10, 0);
    mpg_dot = ui_dot(mpg_btn, 11, C_DIM);
    mpg_lbl = ui_label(mpg_btn, "—", ui_f16, C_TEXT);

    // pozycja robocza
    lv_obj_t *pos = ui_card(scr);
    lv_obj_set_flex_grow(pos, 1);
    lv_obj_set_flex_align(pos, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    ui_label(pos, "POZYCJA ROBOCZA · mm", ui_f16, C_MUTED);
    lv_obj_t *rx = ui_row(pos, 0);
    ui_label(rx, "X", ui_f32, C_AMBER);
    x_lbl = ui_label(rx, "0.000", ui_f68, C_TEXT);
    lv_obj_t *ry = ui_row(pos, 0);
    ui_label(ry, "Y", ui_f32, C_AMBER);
    y_lbl = ui_label(ry, "0.000", ui_f68, C_TEXT);
    mach_lbl = ui_label(pos, "Maszyna: –", ui_f16, C_MUTED);

    // posuw / moc
    lv_obj_t *cards = ui_row(scr, 0);
    small_card(cards, "POSUW · mm/min", &feed_lbl);
    small_card(cards, "MOC LASERA", &pwr_lbl);

    lv_obj_t *lrow = ui_card(scr);
    lv_obj_set_style_pad_ver(lrow, 0, 0);
    lv_obj_set_height(lrow, 60);
    lv_obj_set_flex_flow(lrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lrow, 12, 0);
    laser_dot = ui_dot(lrow, 14, C_DIM);
    laser_lbl = ui_label(lrow, "Laser wyłączony", ui_f20, C_MUTED);

    // przyciski
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_remove_style_all(grid);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_height(grid, 270);
    lv_obj_set_style_pad_gap(grid, 12, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    tile(grid, LV_SYMBOL_HOME, "Bazuj", home_cb, NULL, 0, 1, 0);
    tile(grid, LV_SYMBOL_SHUFFLE, "Ruch", nav_cb, (void *)(intptr_t)SCR_JOG, 1, 1, 0);
    tile(grid, LV_SYMBOL_GPS, "Zero XY", zero_cb, NULL, 0, 1, 1);
    tile(grid, LV_SYMBOL_SD_CARD, "Pliki", nav_cb, (void *)(intptr_t)SCR_FILES, 1, 1, 1);
    tile(grid, LV_SYMBOL_SETTINGS, "Ustawienia", nav_cb, (void *)(intptr_t)SCR_SETTINGS, 0, 2, 2);
    return scr;
}

void ui_main_update(const grbl_status_t *st)
{
    cur = *st;
    char b[48];
    ui_pill_set(pill, st);

    if (st->mpg == 1)      { lv_obj_set_style_bg_color(mpg_dot, C_AMBER, 0); lv_label_set_text(mpg_lbl, "Panel steruje"); }
    else if (st->mpg == 0) { lv_obj_set_style_bg_color(mpg_dot, C_BLUE, 0);  lv_label_set_text(mpg_lbl, "Komputer steruje"); }
    else                   { lv_obj_set_style_bg_color(mpg_dot, C_DIM, 0);   lv_label_set_text(mpg_lbl, "Sterowanie: ?"); }

    ui_fmt_mm(b, sizeof b, st->wx); lv_label_set_text(x_lbl, b);
    ui_fmt_mm(b, sizeof b, st->wy); lv_label_set_text(y_lbl, b);
    snprintf(b, sizeof b, "Maszyna: %.3f · %.3f", (double)st->mx, (double)st->my);
    lv_label_set_text(mach_lbl, b);

    snprintf(b, sizeof b, "%d", (int)st->feed);
    lv_label_set_text(feed_lbl, b);
    snprintf(b, sizeof b, "S%d", (int)st->spindle);
    lv_label_set_text(pwr_lbl, b);

    lv_obj_set_style_bg_color(laser_dot, st->laser_on ? C_AMBER : C_DIM, 0);
    lv_label_set_text(laser_lbl, st->laser_on ? "Laser włączony" : "Laser wyłączony");
    lv_obj_set_style_text_color(laser_lbl, st->laser_on ? C_AMBER : C_MUTED, 0);
}

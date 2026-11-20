#include <stdio.h>

#include "ui_common.h"

static const float STEPS[] = {0.1f, 1.0f, 10.0f, 100.0f, 0.0f};   // 0 = ruch ciągły (dopóki trzymasz)
static const char *STEP_TXT[] = {"0.1", "1", "10", "100", "Ciągły"};
static const int FEEDS[] = {300, 1500, 3000};
#define NSTEP 5
#define NFEED 3

static int s_step = 2, s_feed = 1;       // pamiętane między wejściami na ekran
static lv_obj_t *pos_lbl, *step_btn[NSTEP], *feed_btn[NFEED];
static grbl_status_t cur;

// kierunki w siatce 3x3 (wiersz po wierszu); {0,0} = STOP
static const int DIRS[9][2] = {{-1, 1}, {0, 1}, {1, 1}, {-1, 0}, {0, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}};
static const char *DIR_TXT[9] = {"X− Y+", "Y+", "X+ Y+", "X−", "STOP", "X+", "X− Y−", "Y−", "X+ Y−"};
static const char *DIR_SYM[9] = {NULL, LV_SYMBOL_UP, NULL, LV_SYMBOL_LEFT, NULL, LV_SYMBOL_RIGHT, NULL, LV_SYMBOL_DOWN, NULL};

static lv_timer_t *s_hold_timer;
static lv_obj_t *fire_btn;
static lv_timer_t *s_arm_timer;          // test lasera: pierwsze dotknięcie uzbraja na 3 s

#define FIRE_S     10                    // moc testu (S10 = 1 % przy $30=1000)
#define FIRE_SEC   1                     // czas impulsu w s (odmierza płyta przez G4)
#define FIRE_TXT   "Test lasera"

static void fire_disarm(void);

static int s_hold_dx, s_hold_dy;

static void send_jog(int dx, int dy, float dist, bool absolute_zero)
{
    char b[96];
    int n;
    if (absolute_zero) n = snprintf(b, sizeof b, "$J=G90 G21 X0 Y0 F%d", FEEDS[s_feed]);
    else {
        n = snprintf(b, sizeof b, "$J=G91 G21");
        if (dx) n += snprintf(b + n, sizeof b - n, " X%.3f", (double)(dx * dist));
        if (dy) n += snprintf(b + n, sizeof b - n, " Y%.3f", (double)(dy * dist));
        snprintf(b + n, sizeof b - n, " F%d", FEEDS[s_feed]);
    }
    grbl_send_line(b);
}

static void hold_tick(lv_timer_t *t)
{
    // odcinek ok. 0,1 s ruchu przy wybranym posuwie, wysyłany co ~0,12 s
    float seg = FEEDS[s_feed] / 60.0f * 0.12f;
    send_jog(s_hold_dx, s_hold_dy, seg, false);
}

static void hold_stop(void)
{
    if (s_hold_timer) { lv_timer_del(s_hold_timer); s_hold_timer = NULL; grbl_send_realtime((char)0x85); }
}

static void dir_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    int dx = DIRS[i][0], dy = DIRS[i][1];

    if (dx == 0 && dy == 0) {            // STOP
        if (code == LV_EVENT_CLICKED) { hold_stop(); fire_disarm(); grbl_send_realtime((char)0x85); }
        return;
    }
    bool cont = STEPS[s_step] == 0.0f;
    if (code == LV_EVENT_CLICKED && !cont) {
        if (ui_can_command(&cur)) send_jog(dx, dy, STEPS[s_step], false);
    } else if (code == LV_EVENT_PRESSED && cont) {
        if (!ui_can_command(&cur)) return;
        s_hold_dx = dx; s_hold_dy = dy;
        hold_tick(NULL);
        if (!s_hold_timer) s_hold_timer = lv_timer_create(hold_tick, 120, NULL);
    } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && cont) {
        hold_stop();
    }
}

static void step_cb(lv_event_t *e)
{
    s_step = (int)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < NSTEP; i++) ui_button_selected(step_btn[i], i == s_step);
}

static void feed_cb(lv_event_t *e)
{
    s_feed = (int)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < NFEED; i++) ui_button_selected(feed_btn[i], i == s_feed);
}

static void zero_cb(lv_event_t *e)
{
    if (ui_can_command(&cur)) grbl_send_line("G10 L20 P0 X0 Y0");
}

static void to_zero_cb(lv_event_t *e)
{
    if (ui_can_command(&cur)) send_jog(0, 0, 0, true);
}

static void fire_disarm(void)
{
    if (s_arm_timer) { lv_timer_del(s_arm_timer); s_arm_timer = NULL; }
    if (fire_btn) {
        ui_button_selected(fire_btn, false);       // przywraca kolory zwykłego przycisku
        lv_obj_set_style_bg_color(fire_btn, C_BTN_PRESS, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(fire_btn, 1, 0);
        lv_label_set_text(ui_button_label(fire_btn), FIRE_TXT);
    }
}

static void fire_arm_timeout(lv_timer_t *t)
{
    s_arm_timer = NULL;      // timer jednorazowy - LVGL usuwa go sam
    fire_disarm();
}

static void fire_cb(lv_event_t *e)
{
    if (!ui_can_command(&cur)) return;
    if (cur.state_id != GRBL_ST_IDLE) { ui_toast("Test lasera tylko w stanie IDLE"); fire_disarm(); return; }
    if (!s_arm_timer) {      // pierwsze dotknięcie: uzbrojenie
        ui_button_danger(fire_btn);
        lv_label_set_text(ui_button_label(fire_btn), "Dotknij - laser!");
        s_arm_timer = lv_timer_create(fire_arm_timeout, 3000, NULL);
        lv_timer_set_repeat_count(s_arm_timer, 1);
        return;
    }
    fire_disarm();
    // Tryb lasera ($32=1): M3 przy aktywnym G1 świeci bez ruchu; G4 trzyma impuls po stronie płyty,
    // więc M5 wykona się nawet przy zerwanym łączu z panelem.
    char b[32];
    grbl_send_line("G1 F100");
    snprintf(b, sizeof b, "M3 S%d", FIRE_S);
    grbl_send_line(b);
    snprintf(b, sizeof b, "G4 P%d", FIRE_SEC);
    grbl_send_line(b);
    grbl_send_line("M5");
    grbl_send_line("G0");
    ui_toast("Laser S%d przez %d s", FIRE_S, FIRE_SEC);
}

static void gone_cb(lv_event_t *e)
{
    fire_disarm();
    fire_btn = NULL;
    hold_stop();    // ekran usuwany w trakcie trzymania - nie zostawiaj timera ani ruchu
}

lv_obj_t *ui_jog_create(void)
{
    lv_obj_t *scr = ui_new_screen();
    lv_obj_add_event_cb(scr, gone_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t *right;
    ui_header(scr, "Ruch ręczny", SCR_MAIN, &right);
    pos_lbl = ui_label(right, "X 0.000\nY 0.000", ui_f20, C_TEXT);
    lv_obj_set_style_text_align(pos_lbl, LV_TEXT_ALIGN_RIGHT, 0);

    static lv_coord_t c3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t r3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_t *pad = lv_obj_create(scr);
    lv_obj_remove_style_all(pad);
    lv_obj_set_width(pad, LV_PCT(100));
    lv_obj_set_flex_grow(pad, 1);
    lv_obj_set_style_pad_gap(pad, 8, 0);
    lv_obj_set_layout(pad, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(pad, c3, r3);
    for (int i = 0; i < 9; i++) {
        lv_obj_t *b = ui_button(pad, NULL, ui_f20, NULL, NULL);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
        bool diag = DIRS[i][0] && DIRS[i][1];
        bool stop = !DIRS[i][0] && !DIRS[i][1];
        if (stop) {
            lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(b, 3, 0);
            lv_obj_set_style_border_color(b, C_RED, 0);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x2A1416), 0);
            lv_obj_set_style_text_color(b, C_RED_TXT, 0);
        } else if (diag) {
            lv_obj_set_style_bg_color(b, C_CARD, 0);
            lv_obj_set_style_text_color(b, C_MUTED, 0);
        } else {
            lv_obj_set_style_bg_color(b, C_BTN2, 0);
        }
        if (DIR_SYM[i]) {
            lv_obj_t *s = lv_label_create(b);
            lv_label_set_text(s, DIR_SYM[i]);
            lv_obj_set_style_text_font(s, &lv_font_montserrat_32, 0);
            lv_obj_set_style_text_color(s, C_AMBER, 0);
        }
        lv_obj_t *t = lv_label_create(b);
        lv_label_set_text(t, DIR_TXT[i]);
        lv_obj_set_style_text_font(t, diag ? ui_f16 : ui_f20, 0);
        lv_obj_add_event_cb(b, dir_cb, LV_EVENT_ALL, (void *)(intptr_t)i);
    }

    lv_obj_t *c1 = ui_col(scr);
    lv_obj_set_width(c1, LV_PCT(100));
    lv_obj_set_style_pad_row(c1, 8, 0);
    ui_label(c1, "KROK · mm", ui_f16, C_MUTED);
    lv_obj_t *r1 = ui_row(c1, 64);
    lv_obj_set_style_pad_column(r1, 8, 0);
    for (int i = 0; i < NSTEP; i++) {
        step_btn[i] = ui_button(r1, STEP_TXT[i], i == 4 ? ui_f16 : ui_f20, step_cb, (void *)(intptr_t)i);
        lv_obj_set_flex_grow(step_btn[i], 1);
        lv_obj_set_height(step_btn[i], 64);
        ui_button_selected(step_btn[i], i == s_step);
    }

    lv_obj_t *c2 = ui_col(scr);
    lv_obj_set_width(c2, LV_PCT(100));
    lv_obj_set_style_pad_row(c2, 8, 0);
    ui_label(c2, "POSUW · mm/min", ui_f16, C_MUTED);
    lv_obj_t *r2 = ui_row(c2, 64);
    lv_obj_set_style_pad_column(r2, 8, 0);
    for (int i = 0; i < NFEED; i++) {
        char t[8];
        snprintf(t, sizeof t, "%d", FEEDS[i]);
        feed_btn[i] = ui_button(r2, t, ui_f20, feed_cb, (void *)(intptr_t)i);
        lv_obj_set_flex_grow(feed_btn[i], 1);
        lv_obj_set_height(feed_btn[i], 64);
        ui_button_selected(feed_btn[i], i == s_feed);
    }

    lv_obj_t *r3b = ui_row(scr, 68);
    lv_obj_set_style_pad_column(r3b, 8, 0);
    lv_obj_t *b1 = ui_button(r3b, "Zero XY", ui_f20, zero_cb, NULL);
    lv_obj_set_flex_grow(b1, 1); lv_obj_set_height(b1, 68);
    lv_obj_t *b2 = ui_button(r3b, "Do zera", ui_f20, to_zero_cb, NULL);
    lv_obj_set_flex_grow(b2, 1); lv_obj_set_height(b2, 68);
    fire_btn = ui_button(r3b, FIRE_TXT, ui_f20, fire_cb, NULL);
    lv_obj_set_flex_grow(fire_btn, 1); lv_obj_set_height(fire_btn, 68);
    return scr;
}

void ui_jog_update(const grbl_status_t *st)
{
    cur = *st;
    char b[64];
    snprintf(b, sizeof b, "X %.3f\nY %.3f", (double)st->wx, (double)st->wy);
    lv_label_set_text(pos_lbl, b);
}

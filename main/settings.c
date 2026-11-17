#include "settings.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grbl_link.h"

const char *const SET_CAT_NAMES[SC_COUNT] = {"Laser", "Ruch", "Bazowanie", "Granice", "Panel"};

const set_def_t SET_DEFS[] = {
    // Laser
    {32,  SC_LASER,  SK_BIT, 0, "Tryb lasera", ""},
    {30,  SC_LASER,  SK_NUM, 0, "Maks. moc (S)", ""},
    {31,  SC_LASER,  SK_NUM, 0, "Min. moc (S)", ""},
    {33,  SC_LASER,  SK_NUM, 0, "Częstotliwość PWM", "Hz"},
    {16,  SC_LASER,  SK_BIT, 2, "Odwróć sygnał PWM", ""},
    // Ruch
    {100, SC_MOTION, SK_NUM, 0, "Kroków na mm – X", "kr/mm"},
    {101, SC_MOTION, SK_NUM, 0, "Kroków na mm – Y", "kr/mm"},
    {110, SC_MOTION, SK_NUM, 0, "Prędkość maks. – X", "mm/min"},
    {111, SC_MOTION, SK_NUM, 0, "Prędkość maks. – Y", "mm/min"},
    {120, SC_MOTION, SK_NUM, 0, "Przyspieszenie – X", "mm/s²"},
    {121, SC_MOTION, SK_NUM, 0, "Przyspieszenie – Y", "mm/s²"},
    // Bazowanie
    {22,  SC_HOMING, SK_BIT, 0, "Bazowanie włączone", ""},
    {23,  SC_HOMING, SK_NUM, 0, "Maska kierunku bazowania", ""},
    {24,  SC_HOMING, SK_NUM, 0, "Posuw dokładny", "mm/min"},
    {25,  SC_HOMING, SK_NUM, 0, "Posuw szukania", "mm/min"},
    {26,  SC_HOMING, SK_NUM, 0, "Debounce krańcówek", "ms"},
    {27,  SC_HOMING, SK_NUM, 0, "Odskok po bazowaniu", "mm"},
    // Granice
    {21,  SC_LIMITS, SK_BIT, 0, "Twarde krańcówki", ""},
    {20,  SC_LIMITS, SK_BIT, 0, "Miękkie granice", ""},
    {130, SC_LIMITS, SK_NUM, 0, "Maks. ruch – X", "mm"},
    {131, SC_LIMITS, SK_NUM, 0, "Maks. ruch – Y", "mm"},
    {5,   SC_LIMITS, SK_NUM, 0, "Odwrócenie krańcówek (maska)", ""},
};
const int SET_DEF_COUNT = sizeof SET_DEFS / sizeof SET_DEFS[0];

static float s_val[sizeof SET_DEFS / sizeof SET_DEFS[0]];
static bool s_have[sizeof SET_DEFS / sizeof SET_DEFS[0]];
static volatile uint32_t s_version;
static volatile bool s_pending, s_loaded, s_got_line;
static volatile int s_write_err;
static volatile int s_write_pending;

static int index_of(const set_def_t *d) { return (int)(d - SET_DEFS); }

static void on_line(const char *line, void *ctx)
{
    if (s_pending) {
        if (line[0] == '$') {
            unsigned id;
            float v;
            if (sscanf(line, "$%u=%f", &id, &v) == 2) {
                s_got_line = true;
                for (int i = 0; i < SET_DEF_COUNT; i++)
                    if (SET_DEFS[i].id == id) { s_val[i] = v; s_have[i] = true; }
            }
        } else if (!strcmp(line, "ok") && s_got_line) {
            s_pending = false;
            s_loaded = true;
            s_version++;
        }
    }
}

static void on_resp(bool ok, int err, void *ctx)
{
    if (s_write_pending > 0 && !s_pending) {
        s_write_pending--;
        if (!ok) s_write_err = err;
    }
}

void settings_init(void)
{
    grbl_link_add_line_cb(on_line, NULL);
    grbl_link_add_response_cb(on_resp, NULL);
}

void settings_refresh(void)
{
    s_got_line = false;
    s_pending = true;
    grbl_send_line("$$");
}

bool settings_loaded(void) { return s_loaded; }
uint32_t settings_version(void) { return s_version; }

bool settings_get(const set_def_t *d, float *val)
{
    int i = index_of(d);
    if (!s_have[i]) return false;
    *val = d->kind == SK_BIT ? (float)(((int)lroundf(s_val[i]) >> d->bit) & 1) : s_val[i];
    return true;
}

bool settings_write(const set_def_t *d, float val)
{
    int i = index_of(d);
    float out = val;
    if (d->kind == SK_BIT) {
        if (!s_have[i]) return false;     // bez znanej wartości nie wolno zgadywać pozostałych bitów
        int cur = (int)lroundf(s_val[i]);
        out = (float)(val != 0 ? (cur | (1 << d->bit)) : (cur & ~(1 << d->bit)));
    }
    char buf[40];
    snprintf(buf, sizeof buf, "$%u=%g", d->id, (double)out);
    s_write_err = 0;
    s_write_pending++;
    if (!grbl_send_line(buf)) { s_write_pending--; return false; }
    s_val[i] = out;
    s_have[i] = true;
    s_version++;
    return true;
}

int settings_take_error(void)
{
    int e = s_write_err;
    s_write_err = 0;
    return e;
}

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Ustawienia grblHAL wyświetlane na panelu: odczyt przez "$$", zapis "$n=wartość".

typedef enum { SC_LASER, SC_MOTION, SC_HOMING, SC_LIMITS, SC_PANEL, SC_COUNT } set_cat_t;
typedef enum { SK_NUM, SK_BIT } set_kind_t;   // SK_BIT: przełącznik jednego bitu wartości

typedef struct {
    uint16_t id;
    uint8_t cat;
    uint8_t kind;
    uint8_t bit;           // dla SK_BIT
    const char *label;     // po polsku
    const char *unit;      // może być ""
} set_def_t;

extern const set_def_t SET_DEFS[];
extern const int SET_DEF_COUNT;
extern const char *const SET_CAT_NAMES[SC_COUNT];

void settings_init(void);                 // rejestruje odbiór linii z łącza
void settings_refresh(void);              // wysyła "$$" (tylko gdy nic innego nie trwa)
bool settings_loaded(void);
uint32_t settings_version(void);          // zwiększana po każdej zmianie cache
bool settings_get(const set_def_t *d, float *val);   // dla SK_BIT zwraca 0/1; false gdy nie wczytano
bool settings_write(const set_def_t *d, float val);  // zapis do maszyny; dla SK_BIT val = 0/1
int  settings_take_error(void);           // numer błędu z ostatniego zapisu (0 = brak), zeruje

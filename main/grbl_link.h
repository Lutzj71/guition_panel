#pragma once

#include <stdbool.h>
#include <stdint.h>

// Łącze z grblHAL po UART (USART3 płyty, MPG_ENABLE=2, 115200 8N1).

typedef enum {
    GRBL_ST_UNKNOWN, GRBL_ST_IDLE, GRBL_ST_RUN, GRBL_ST_HOLD, GRBL_ST_JOG,
    GRBL_ST_ALARM, GRBL_ST_HOME, GRBL_ST_DOOR, GRBL_ST_CHECK, GRBL_ST_SLEEP,
} grbl_state_t;

typedef struct {
    bool valid;            // odebrano co najmniej jeden raport statusu
    bool stale;            // brak raportu od ponad 1,5 s
    char state[16];        // tekst z raportu: Idle, Run, Hold:0, Jog, Alarm, Home, ...
    grbl_state_t state_id;
    float mx, my;          // pozycja maszynowa
    float wx, wy;          // pozycja robocza (MPos - WCO)
    float feed;            // mm/min
    float spindle;         // S (moc lasera)
    int ov_feed, ov_rapid, ov_spindle;   // korekty w % (domyślnie 100)
    bool laser_on;         // pole A: zawiera S lub C
    int mpg;               // -1 nieznany, 0 steruje host, 1 steruje panel
    int alarm;             // ostatni ALARM:n, 0 = brak
    int last_error;        // ostatni error:n
    uint32_t msg_seq;      // zwiększany przy każdym ok / error / ALARM
} grbl_status_t;

typedef void (*grbl_resp_cb_t)(bool ok, int error, void *ctx);     // dla "ok" i "error:n"
typedef void (*grbl_line_cb_t)(const char *line, void *ctx);       // dla pozostałych linii (nie raportów)

void grbl_link_init(void);
void grbl_link_get_status(grbl_status_t *out);

// Rejestracja odbiorców (maks. 4 każdego rodzaju). Wywołania z zadania odbiorczego UART.
void grbl_link_add_response_cb(grbl_resp_cb_t cb, void *ctx);
void grbl_link_add_line_cb(grbl_line_cb_t cb, void *ctx);

// Linia G-code lub komenda ($H, $X, $J=...), kończona '\n' przez funkcję (bezpieczne wątkowo).
bool grbl_send_line(const char *line);
// Znak czasu rzeczywistego bez końca linii: '!', '~', 0x18, 0x85 (anulowanie jogu), korekty 0x90..0x9D.
void grbl_send_realtime(char c);

// Przełączanie sterowania MPG (znak 0x8B przełącza tryb – nie ma komendy "ustaw").
void grbl_mpg_toggle(void);
// Wysyła 0x8B tylko gdy znany stan różni się od żądanego (albo jest nieznany).
void grbl_mpg_request(bool panel_controls);

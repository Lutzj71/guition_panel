#pragma once

#include <stdbool.h>

// Łącze z grblHAL po UART (USART3 płyty, MPG_ENABLE=2, 115200 8N1).

typedef struct {
    bool valid;          // odebrano co najmniej jeden raport statusu
    bool stale;          // brak raportu od ponad 1,5 s
    char state[16];      // Idle, Run, Hold:0, Jog, Alarm, Home, ...
    float x, y;          // MPos lub WPos, zależnie od $10
    bool wpos;           // true = współrzędne robocze
    float feed;          // mm/min
    float spindle;       // S (moc lasera)
    char last_msg[64];   // ostatni komunikat ok / error:N / ALARM:N
} grbl_status_t;

void grbl_link_init(void);
void grbl_link_get_status(grbl_status_t *out);

// Linia G-code lub komenda ($H, $X, $J=...), kończona '\n' przez funkcję.
bool grbl_send_line(const char *line);
// Znak czasu rzeczywistego bez końca linii: '!', '~', 0x18, 0x85 (anulowanie jogu), 0x8B (przełączenie MPG).
void grbl_send_realtime(char c);

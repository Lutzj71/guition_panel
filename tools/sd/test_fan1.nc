; Test: ktora komenda steruje wyjsciem FAN1 plyty Creality V4.2.7 (grblHAL).
; Na FAN1 podlacz multimetr (napiecie miedzy + i -) albo diode LED z rezystorem.
; Bez ruchu osi. Laser NIE powinien byc podlaczony.
;
;  0-3 s  : M3 (spindle enable, PC7)  -> FAN1 wlaczone = steruje M3/M5
;  3-5 s  : wszystko wylaczone (przerwa)
;  5-8 s  : M7 (chlodziwo mist, PA0)  -> FAN1 wlaczone = steruje M7/M9

G21
G90
G1 F100        ; tryb G1, zeby w trybie lasera M3 nie bylo blokowane przez G0
M3 S1          ; S1 = 0,1 % PWM (na wypadek, gdyby cos wisialo na wyjsciu PWM)
G4 P3
M5
G4 P2
M7
G4 P3
M9
G0

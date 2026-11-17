# guition_panel

Firmware panelu dotykowego **Guition JC4880P443C_I_W** (ESP32-P4, 4,3", 480×800 pion, ST7701S, GT911)
do wypalarki laserowej sterowanej przez grblHAL na płycie Creality V4.2.7
(firmware płyty: `..\grblHAL_STM32F1xx`, obudowy: `..\laser_enclosure`).

**Stan: szkielet, nie kompilowany i nie testowany.** W systemie nie ma ESP-IDF, a PlatformIO
(`espressif32@6.12.0`) nie obsługuje ESP32-P4, więc kodu nie dało się zbudować. Pierwsza kompilacja
może wymagać poprawek API.

## Co jest

- `main/board.c` – LDO PHY DSI, ręczny init ST7701S (tablica z opisu społecznościowego płyty),
  DSI/DPI 480×800 RGB565, dotyk GT911 (I²C GPIO7/8), LVGL 8 przez `esp_lvgl_port`.
- `main/grbl_link.c` – UART do grblHAL: odpytywanie `?` co 200 ms, parser raportów statusu
  (stan, MPos/WPos, FS), ostatni `ok` / `error:` / `ALARM:`, wysyłanie linii i znaków czasu rzeczywistego.
- `main/ui.c` – jeden ekran: stan, pozycja X/Y, krok jogu (0,1 / 1 / 10 / 50 mm), krzyżak jogu,
  Home, Unlock, Hold, Resume, Reset.

Czego **nie ma** względem makiety (6 ekranów, artefakt: https://claude.ai/artifact/QaSk7QgnDGDfL8GGXo8aZG):
pozostałych ekranów, wyboru pliku z karty SD, strumieniowania G-code, ustawień grblHAL (`$$`),
polskich znaków (czcionki Montserrat ich nie mają – napisy są bez ogonków), Wi-Fi (ESP32-C6 nieużywany).
Z premedytacją nie ma też przycisku testu lasera.

## Budowanie

Wymaga ESP-IDF ≥ 5.4.2 (zalecane 5.5):

```
idf.py set-target esp32p4
idf.py build
idf.py -p COMx flash
```

Komponenty (`lvgl` 8.4, `esp_lvgl_port`, `esp_lcd_touch_gt911`) pobiera menedżer komponentów
(`main/idf_component.yml`). Układ z płyty ma rewizję v1.x (engineering sample) – stąd
`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` w `sdkconfig.defaults`.

## Połączenie z płytą Creality

| Panel (CN2, MX1.25 4P) | Płyta Creality (złącze LCD) |
|---|---|
| 1 VIN 5 V | 5 V |
| 2 TX0 (GPIO37) | RX USART3 = PB11 |
| 3 RX0 (GPIO38) | TX USART3 = PB10 |
| 4 GND | GND |

Po stronie grblHAL: `MPG_ENABLE=2`, USART3, 115200. Przełączanie sterowania MPG znakiem `0x8B`
– szkielet go jeszcze nie wysyła (patrz niżej). Poziomy logiczne: STM32F103 i ESP32-P4 to 3,3 V –
sprawdź, czy wyprowadzenia 5 V na złączu LCD płyty nie niosą też sygnałów 5 V.

## Do sprawdzenia przy pierwszym uruchomieniu

1. **Czarny ekran** – sprawdź tablicę init ST7701S i kolejność: reset GPIO5 → tablica → `esp_lcd_panel_init`.
   Sterownik `esp_lcd_st7701` z rejestru na tej płycie nie działa.
2. **Kierunki dotyku** – w `board.c` przyjęto `mirror_y = 1`; dotknij lewego górnego i prawego dolnego
   rogu i popraw `swap_xy` / `mirror_x` / `mirror_y`.
3. **Piny dotyku RST/INT** – schemat podaje GPIO22/21, przykłady Arduino `-1/-1`. Jeśli GT911 nie
   odpowiada (skan I²C: adres 0x5D), spróbuj `GPIO_NUM_NC`.
4. **UART0 jest współdzielony** – GPIO37/38 to jednocześnie złącze CN2 i konsola/flashowanie UART0.
   Przy podłączonej płycie Creality flashowanie przez UART0 może się kłócić z jej TX; konsolę logów
   przeniesiono na USB-Serial-JTAG, ale nie sprawdziłem, czy to złącze USB-C jest wyprowadzone.
   Komunikaty ROM z rozruchu pójdą na GPIO37 do płyty – grblHAL powinien je zignorować.
5. **MPG** – w `MPG_ENABLE=2` grblHAL oddaje sterowanie panelowi po `0x8B`. Przełączanie jest
   odwracane przy kolejnym `0x8B`, więc trzeba wysłać go raz i sprawdzić stan (np. po odpowiedzi na `?`).
   Do dopisania w `grbl_link.c`.
6. **Jog** – `$J=G91 G21 X.. F3000` (`JOG_FEED` w `ui.c`); sprawdź, czy kierunki zgadzają się z osiami
   maszyny, zanim podłączysz laser.
7. **Piny 5 V na JP1** są wyjściem, nie wejściem – zasilaj panel tylko przez CN2 albo USB-C.

## Źródła pinów

github.com/ultramcu/guition-jc4880p443c-i-w oraz github.com/seobaeksol/JC4880P443C_I_W
(opisy społecznościowe, nie dokumentacja producenta; pełny pakiet producenta jest na stronie
`pan.jczn1688.com`, patrz README tamtego repozytorium).

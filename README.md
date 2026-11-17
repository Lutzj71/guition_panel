# guition_panel

Firmware panelu dotykowego **Guition JC4880P443C_I_W** (ESP32-P4, 4,3", 480×800 pion, ST7701S, GT911)
do wypalarki laserowej sterowanej przez grblHAL na płycie Creality V4.2.7
(firmware płyty: `..\grblHAL_STM32F1xx`, obudowy: `..\laser_enclosure`).

**Stan: kod kompletny wg makiety, ale niezbudowany i nietestowany na sprzęcie.** ESP-IDF nie było
dostępne w tej sesji. Sprawdzono tylko składnię i użycie API LVGL 8.4 (kompilator `-fsyntax-only`
względem nagłówków LVGL) dla wszystkich plików interfejsu oraz `grbl_link.c` i `gcode_stream.c`
(na zaślepkach nagłówków ESP-IDF). Pierwsza pełna kompilacja może wymagać poprawek API ESP-IDF
(`board.c`, `sdcard.c`, `wifi_mgr.c`, `http_upload.c`).

## Ekrany (makieta: https://claude.ai/artifact/QaSk7QgnDGDfL8GGXo8aZG)

| Plik | Ekran |
|---|---|
| `ui_main.c` | Główny: stan, pozycja robocza X/Y, posuw, moc, laser, Bazuj / Ruch / Zero XY / Pliki / Ustawienia, przełącznik sterowania (MPG) |
| `ui_jog.c` | Ruch ręczny: siatka 3×3 z przekątnymi, STOP, krok 0,1 / 1 / 10 / 100 / ciągły, posuw 300 / 1500 / 3000, Zero XY, Do zera |
| `ui_run.c` | Praca: pozycja, posuw, moc, pasek postępu zadania z karty SD, korekty posuwu i mocy ±10 %, Pauza/Wznów, Stop |
| `ui_settings.c` | Ustawienia grblHAL (zakładki Laser / Ruch / Bazowanie / Granice / Panel) + Edycja wartości z klawiaturą numeryczną |
| `ui_alarm.c` | Alarm: opis po polsku, Bazuj / Odblokuj / Reset |
| `ui_files.c` | Karta SD: lista plików G-code, potwierdzenie i start zadania |
| `ui_wifi.c` | Wi-Fi: skanowanie, hasło na klawiaturze ekranowej, adres IP |

Ekrany przełączają się same: alarm → Alarm, praca (zadanie z panelu lub z komputera) → Praca.
Polskie znaki: czcionka Montserrat Medium (SIL OFL) osadzona w firmware (`fonts/`), renderowana
przez TinyTTF; ikony pochodzą z czcionki wbudowanej LVGL.

## Moduły

- `grbl_link.c` – UART do grblHAL, odpytywanie `?` co 200 ms; parser raportów (stan, MPos/WPos, WCO,
  FS, Ov, A, MPG), `ok` / `error:n` / `ALARM:n`, wysyłanie linii i znaków czasu rzeczywistego.
- `gcode_stream.c` – strumieniowanie pliku z karty metodą *character counting* (okno 120 B, do 40 linii
  w locie); usuwa komentarze i spacje; pauza `!`, wznowienie `~`, stop = `!` + reset 0x18;
  błąd `error:n` wstrzymuje wysyłanie i pokazuje błędną linię.
- `settings.c` – odczyt `$$`, zapis `$n=v`, przełączniki pojedynczych bitów z zachowaniem pozostałych.
- `sdcard.c` – SDMMC 4-bit (slot 0, piny 39–44), zasilanie karty z LDO VO4.
- `wifi_mgr.c`, `http_upload.c` – Wi-Fi przez ESP32-C6 (ESP-Hosted), dane sieci w NVS;
  po połączeniu serwer HTTP: strona z formularzem pod `http://<IP>/` i `PUT /upload/<nazwa>`
  (np. `curl -T plik.nc http://<IP>/upload/plik.nc`). **Bez uwierzytelniania – tylko sieć domowa.**
  Nazwy plików: litery, cyfry, `.` `_` `-`.

## Budowanie

Wymaga ESP-IDF ≥ 5.4.2 (zalecane 5.5):

```
idf.py set-target esp32p4
idf.py build
idf.py -p COMx flash
```

Komponenty (`lvgl` 8.4, `esp_lvgl_port`, `esp_lcd_touch_gt911`, `esp_wifi_remote`) pobiera menedżer
komponentów (`main/idf_component.yml`). Układ z płyty ma rewizję v1.x (engineering sample) – stąd
`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` w `sdkconfig.defaults`.

### Wi-Fi: przed pierwszym użyciem

1. **Firmware ESP32-C6** (slave ESP-Hosted) na płycie bywa w wersji 2.3.0, a host oczekuje ~2.12 –
   wtedy Wi-Fi łączy i rozłącza się w pętli. Procedura aktualizacji przez UART0 C6 na złączu JP1 jest
   w README https://github.com/ultramcu/guition-jc4880p443c-i-w (sekcja 1). Bez tego ekran Wi-Fi
   pokaże „niedostępne” albo nie utrzyma połączenia.
2. **Piny SDIO do C6** (CLK 18, CMD 19, D0–D3 = 14–17, reset C6 = GPIO54) – ustaw w
   `idf.py menuconfig` → ESP-Hosted (nazwy opcji zależą od wersji `esp_hosted`; nie wpisałem ich do
   `sdkconfig.defaults`, bo nie mogłem ich zweryfikować).

## Połączenie z płytą Creality

| Panel (CN2, MX1.25 4P) | Płyta Creality (złącze LCD) |
|---|---|
| 1 VIN 5 V | 5 V |
| 2 TX0 (GPIO37) | RX USART3 = PB11 |
| 3 RX0 (GPIO38) | TX USART3 = PB10 |
| 4 GND | GND |

Po stronie grblHAL: `MPG_ENABLE=2`, USART3, 115200. Poziomy logiczne: STM32F103 i ESP32-P4 to 3,3 V –
sprawdź, czy wyprowadzenia 5 V na złączu LCD płyty nie niosą też sygnałów 5 V.

## Do sprawdzenia przy pierwszym uruchomieniu

1. **Czarny ekran** – tablica init ST7701S i kolejność: reset GPIO5 → tablica → `esp_lcd_panel_init`.
   Sterownik `esp_lcd_st7701` z rejestru na tej płycie nie działa.
2. **Dotyk** – sprawdzone na sprzęcie: `mirror_y = 0`, `swap_xy = 0`, `mirror_x = 0`. Piny RST/INT GT911
   (GPIO22/21) są poprawne; sterownik musi dostać adres w `driver_data` (inaczej pomija wybór adresu
   i odczyt konfiguracji się nie udaje).
3. **Sterowanie MPG (najważniejsze).** Założenia kodu, których nie mogłem sprawdzić na sprzęcie:
   - znak `0x8B` *przełącza* tryb (nie ma komendy „ustaw”), więc panel wysyła go tylko gdy stan jest
     znany i inny od żądanego, albo – jednorazowo – gdy po 2,5 s od startu brak jakichkolwiek raportów;
   - stan MPG panel odczytuje z pola `MPG:` w raporcie statusu; jeśli Twoja wersja grblHAL go nie
     wysyła, `mpg` zostaje `-1` („Sterowanie: ?”) i przełączasz ręcznie przyciskiem w nagłówku;
   - poza trybem MPG grblHAL może w ogóle nie odpowiadać na tym porcie – wtedy widok „zadanie
     z komputera” (PC steruje) nie będzie miał danych. Do sprawdzenia, czy `$` w grblHAL pozwala
     wysyłać raporty także na strumień MPG.
4. **Jog ciągły** – dopóki trzymasz przycisk, panel wysyła co 120 ms odcinki ≈ 0,1 s ruchu; po
   puszczeniu `0x85`. W buforze UART mogą zostać 1–2 odcinki. Sprawdź na pustej maszynie, zanim
   podłączysz laser.
5. **Strumieniowanie** – okno 120 B jest zachowawcze. Zadanie kończy się, gdy wszystkie linie dostały
   `ok` i maszyna wróciła do Idle. Stop wysyła `!` i reset – wyłącza laser, ale po resecie maszyna może
   wymagać ponownego bazowania (zależnie od `$22`).
6. **Alarmy** – opisy kodów ALARM:n w `ui_alarm.c` spisane z pamięci wg dokumentacji grblHAL;
   zweryfikuj z listą dla swojej wersji.
7. **UART0 jest współdzielony** – GPIO37/38 to jednocześnie złącze CN2 i konsola/flashowanie UART0.
   Przy podłączonej płycie Creality flashowanie przez UART0 może się kłócić z jej TX; logi
   przeniesiono na USB-Serial-JTAG (nie sprawdziłem, czy to złącze USB-C jest wyprowadzone).
   Komunikaty ROM z rozruchu pójdą na GPIO37 do płyty – grblHAL powinien je zignorować.
8. **Karta SD** – FAT, długie nazwy, nawigacja po podkatalogach (ścieżka do 127 znaków, nazwa wpisu do 63);
   rozszerzenia `.nc .gcode .gc .ngc .tap .txt`. Wgrywanie przez HTTP trafia tylko do katalogu głównego.
9. **Pamięć** – czcionki TinyTTF i bufory LVGL są w PSRAM (`CONFIG_SPIRAM_USE_MALLOC=y`).
10. **Piny 5 V na JP1** są wyjściem, nie wejściem – zasilaj panel tylko przez CN2 albo USB-C.

## Źródła pinów

github.com/ultramcu/guition-jc4880p443c-i-w oraz github.com/seobaeksol/JC4880P443C_I_W
(opisy społecznościowe, nie dokumentacja producenta). Czcionka: Montserrat (SIL OFL 1.1).

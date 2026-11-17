# guition_panel – kontekst projektu

Firmware panelu dotykowego do wypalarki laserowej (stan na 2026-11-17). Część większego projektu:

- płyta: `..\grblHAL_STM32F1xx` (Creality V4.2.7, STM32F103RET6, grblHAL; repo Lutzj71/STM32F1xx, gałąź `creality-v427-laser`)
- obudowy: `..\laser_enclosure` (OpenSCAD, profil 2040; repo Lutzj71/laser_enclosure)
- ten folder: ESP-IDF dla panelu **Guition JC4880P443C_I_W** (ESP32-P4 + ESP32-C6, 4,3", 480×800 pion, ST7701S, GT911)

Użytkownik mówi po polsku – odpowiadaj po polsku. Foldery Panlee, Panlee_v3, LVGL_experiment nie mają
z tym projektem nic wspólnego (wcześniej błędnie zakładano panel Smart Panlee 3,5").

## Ustalenia

- ESP-IDF ≥ 5.4.2 (zalecane 5.5), cel `esp32p4`, układ rev v1.x (`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`).
- LVGL 8.4 + esp_lvgl_port; dotyk GT911; Wi-Fi przez ESP32-C6 (esp_wifi_remote / ESP-Hosted).
- Wyświetlacz: ręczna tablica init ST7701S (komponent `esp_lcd_st7701` z rejestru daje czarny ekran).
- Polskie znaki: Montserrat Medium (SIL OFL) osadzony w firmware, renderowany TinyTTF; ikony z wbudowanej czcionki LVGL.
- Łącze z płytą: UART0 na CN2 (GPIO37 TX, GPIO38 RX) ↔ USART3 płyty Creality (PB10/PB11), 115200;
  grblHAL z `MPG_ENABLE=2`, przełączanie znakiem 0x8B (przełącza, nie ustawia).
- Ekrany: Główny, Ruch, Praca, Ustawienia (+edycja), Alarm, Pliki (karta SD), Wi-Fi.
  Makieta: https://claude.ai/artifact/QaSk7QgnDGDfL8GGXo8aZG
- Strumieniowanie G-code z karty SD panelu metodą character counting (okno 120 B); upload plików przez
  HTTP po Wi-Fi (bez uwierzytelniania – tylko sieć domowa).
- Piny: LCD reset GPIO5, podświetlenie GPIO23, I2C SDA7/SCL8, dotyk RST22/INT21, SDMMC slot 0 (CLK43, CMD44,
  D0–D3 = 39–42, zasilanie z LDO VO4), SDIO do C6 (CLK18, CMD19, D0–D3 = 14–17, reset C6 = GPIO54).
  Źródła społecznościowe (ultramcu/guition-jc4880p443c-i-w, seobaeksol/JC4880P443C_I_W), nie producenta.

## Stan

**Kod kompletny wg makiety, ale niezbudowany i nietestowany na sprzęcie.** ESP-IDF nie było zainstalowane
w środowisku (jest tylko rozszerzenie VS Code). Sprawdzono jedynie składnię i API LVGL 8.4
(`arm-none-eabi-gcc -fsyntax-only` na zaślepkach). Pliki ESP-IDF-specyficzne (`board.c`, `sdcard.c`,
`wifi_mgr.c`, `http_upload.c`, nazwy komponentów w `main/CMakeLists.txt`) mogą wymagać poprawek przy
pierwszym `idf.py build`.

Następny krok: zainstalować ESP-IDF, `idf.py set-target esp32p4 && idf.py build`, poprawić błędy.

## Niezweryfikowane założenia (szczegóły w README.md)

- Czy grblHAL wysyła pole `MPG:` w raporcie i czy odpowiada na USART3 poza trybem MPG.
- Orientacja dotyku (`mirror_y=1` założone), tablica init ST7701S, piny RST/INT dotyku.
- Firmware ESP32-C6 trzeba zaktualizować z 2.3.0 do ~2.12; piny SDIO ustawić w menuconfig.
- Opisy kodów ALARM:n w `ui_alarm.c` spisane z pamięci.
- UART0 współdzielony z konsolą/flashowaniem (logi przeniesione na USB-Serial-JTAG).

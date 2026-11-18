# guition_panel – kontekst projektu

Firmware panelu dotykowego do wypalarki laserowej (stan na 2026-11-17). Część większego projektu:

- płyta: `..\STM32F1xx` (Creality V4.2.7, STM32F103RET6, grblHAL; repo Lutzj71/STM32F1xx, gałąź `creality-v427-laser`)
- obudowy: `..\laser_enclosure` (OpenSCAD, profil 2040; repo Lutzj71/laser_enclosure)
- ten folder: ESP-IDF dla panelu **Guition JC4880P443C_I_W** (ESP32-P4 + ESP32-C6, 4,3", 480×800 pion, ST7701S, GT911)

Użytkownik mówi po polsku – odpowiadaj po polsku. Foldery Panlee, Panlee_v3, LVGL_experiment nie mają
z tym projektem nic wspólnego (wcześniej błędnie zakładano panel Smart Panlee 3,5").

## Budowanie i wgrywanie (Windows)

`idf.py` nie ma w PATH. ESP-IDF 5.5.6 zainstalowany przez Espressif Installation Manager: źródła
`C:\esphome-idf\v5.5.6\esp-idf`, narzędzia i venv Pythona `C:\Espressif\tools` (`python\v5.5.6\venv`).
Systemowy Python 3.12 (`%LOCALAPPDATA%\Programs\Python\Python312`) ma esptool; `python` w PATH to alias
Sklepu – używaj pełnej ścieżki albo `py -3.12`.
Skrypt `tools\idf-build.ps1` ustawia środowisko i buduje z **kopii w `C:\gp`** (krótka ścieżka):

```
powershell -ExecutionPolicy Bypass -File tools\idf-build.ps1 -Cmd build
powershell -ExecutionPolicy Bypass -File tools\idf-build.ps1 -Cmd flash -Port COM12
```

Dlaczego kopia: `esp_hosted` ma zagnieżdżone katalogi testowe, a przy ścieżce projektu
`Documents\PlatformIO\...` rozpakowanie kończy się `WinError 206` (MAX_PATH; długie ścieżki wyłączone,
brak admina). Wyniki builda są w `C:\gp\build`, nie w repo. Wymagana zmienna `ESP_IDF_VERSION=5.5`
(inaczej Kconfig `esp_wifi_remote` pomija opcje `CONFIG_WIFI_RMT_*`). Po zmianie `sdkconfig.defaults`
usuń `C:\gp\sdkconfig`, bo wygenerowany plik ma pierwszeństwo.

Port COM panelu zmienia numer po resecie USB (widziano COM12, COM15) i na chwilę znika – sprawdź
`[System.IO.Ports.SerialPort]::GetPortNames()`. Logi: USB-Serial-JTAG, 115200, reset przez RTS.
Panic dekoduj `riscv32-esp-elf-addr2line -pfiaC -e C:\gp\build\guition_panel.elf <adres>`.

Partycja aplikacji (`SINGLE_APP_LARGE`, 0x177000 B) ma wolne tylko ~7% – przy dalszej rozbudowie
potrzebna własna tablica partycji (flash 16 MB).

## Ustalenia

- ESP-IDF ≥ 5.4.2 (zalecane 5.5), cel `esp32p4`, układ rev v1.3 (`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`).
- LVGL 8.4 + esp_lvgl_port; dotyk GT911; Wi-Fi przez ESP32-C6 (esp_wifi_remote / ESP-Hosted).
- Wyświetlacz: ręczna tablica init ST7701S (komponent `esp_lcd_st7701` z rejestru daje czarny ekran) – działa.
- Polskie znaki: Montserrat Medium (SIL OFL) osadzony w firmware, TinyTTF; ikony z wbudowanej czcionki LVGL.
  **`cache_size` w `lv_tiny_ttf_create_data_ex` (LVGL 8.4) jest w BAJTACH**, średnio px² na glif – mała
  wartość daje tablicę haszującą o rozmiarze 0 i crash w `lv_lru_get`. `mk_font` liczy glify × px².
- Łącze z płytą: UART0 na CN2 (GPIO37 TX, GPIO38 RX) ↔ USART3 płyty Creality (PB10/PB11), 115200;
  grblHAL z `MPG_ENABLE=2`, przełączanie znakiem 0x8B (przełącza, nie ustawia).
- Ekrany: Główny, Ruch, Praca, Ustawienia (+edycja), Alarm, Pliki (karta SD, podkatalogi), Wi-Fi.
  Makieta: https://claude.ai/artifact/QaSk7QgnDGDfL8GGXo8aZG
- Strumieniowanie G-code z karty SD metodą character counting (okno 120 B); ścieżka pliku względem karty
  (do 127 znaków, nazwa wpisu do 63). Upload przez HTTP po Wi-Fi tylko do katalogu głównego
  (bez uwierzytelniania – tylko sieć domowa).
- Piny: LCD reset GPIO5, podświetlenie GPIO23, I2C SDA7/SCL8, dotyk RST22/INT21 (adres 0x5D), SDMMC slot 0
  (CLK43, CMD44, D0–D3 = 39–42, zasilanie z LDO VO4), SDIO do C6 (CLK18, CMD19, D0–D3 = 14–17, reset C6 = GPIO54).
  Źródła społecznościowe (ultramcu/guition-jc4880p443c-i-w, seobaeksol/JC4880P443C_I_W), nie producenta.
- Zadanie `sys_evt` ma stos 4096 B (`CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE`); domyślne 2304 B nie
  wystarczało dla handlera Wi-Fi (przepełnienie → pętla restartów).
- GT911: sterownik musi dostać `driver_data` z adresem (`esp_lcd_touch_io_gt911_config_t`), inaczej pomija
  sekwencję wyboru adresu i odczyt konfiguracji się nie udaje.

## Stan

Zbudowane (ESP-IDF 5.5.6) i uruchomione na sprzęcie. **Sprawdzone na panelu:** wyświetlacz, dotyk
(`mirror_y=0`), UI bez restartów, karta SD (FAT) z nawigacją po podkatalogach, start zadania z listy
reaguje (bez maszyny).

**Niesprawdzone:**
- Łącze z grblHAL (MPG): raporty stanu, jog, przełącznik sterowania, strumieniowanie – wymaga podłączenia
  płyty Creality (planowane). Czy grblHAL wysyła pole `MPG:` i odpowiada na USART3 poza trybem MPG.
- Wi-Fi: **działa** (2026-11-18) – C6 zaktualizowany do 2.12.9 przez UART0 na złączu JP1 (`C6_U0RXD`/
  `C6_U0TXD`/GND z konwertera 3,3 V, `C6_IO9` do masy + reset `C6_CHIP_PU`; **przed flashowaniem C6
  zatrzymać P4**, bo resetuje C6 przez GPIO54; obraz z esphome.github.io/esp-hosted-firmware pod 0x10000,
  potem `erase_region 0xd000 0x2000`; README ultramcu, sekcja 1). Host ma esp_hosted 3.1.0, log zgłasza
  `major version mismatch`, ale połączenie i HTTP działają stabilnie. Przy zrywaniu: przypiąć esp_hosted
  2.12.x albo wgrać C6 firmware 3.x. Oszczędzanie energii wyłączone (`WIFI_PS_NONE`) – z modem sleep
  ping ~200 ms i straty.
- Jog ciągły, kompletne zadanie z karty, opisy kodów ALARM:n w `ui_alarm.c` (spisane z pamięci).
- UART0 współdzielony z konsolą/flashowaniem (logi na USB-Serial-JTAG).
- Ostrzeżenie `ldo: voltage value 0` przy montowaniu SD jest kosmetyczne (karta działa).

Szczegółowa lista kontrolna pierwszego uruchomienia: README.md.

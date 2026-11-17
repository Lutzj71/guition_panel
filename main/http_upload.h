#pragma once

// Minimalny serwer HTTP do wgrywania plików G-code na kartę SD przez Wi-Fi:
//   GET /                      strona z formularzem
//   PUT /upload/<nazwa>        treść pliku w ciele żądania (np. curl -T plik.nc http://IP/upload/plik.nc)
// Bez uwierzytelniania - tylko w zaufanej sieci domowej.

void http_upload_start(void);
void http_upload_stop(void);

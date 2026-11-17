#include "http_upload.h"

#include <stdio.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "gcode_stream.h"
#include "sdcard.h"

static const char *TAG = "http";
static httpd_handle_t s_srv;

static const char PAGE[] =
    "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
    "<title>Laser - wgraj plik</title>"
    "<body style='font-family:sans-serif;max-width:480px;margin:2em auto;padding:0 1em'>"
    "<h2>Wgraj plik G-code na kartę SD</h2>"
    "<input type=file id=f accept='.nc,.gcode,.gc,.ngc,.tap,.txt'> <button onclick=up()>Wyślij</button>"
    "<p id=s></p>"
    "<script>async function up(){const f=document.getElementById('f').files[0];"
    "if(!f)return;const s=document.getElementById('s');s.textContent='Wysyłanie...';"
    "const r=await fetch('/upload/'+encodeURIComponent(f.name),{method:'PUT',body:f});"
    "s.textContent=r.ok?'Zapisano: '+f.name:'Błąd '+r.status+': '+await r.text();}</script>";

static esp_err_t get_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

// Dozwolone znaki nazwy: litery, cyfry, . _ - (bez ścieżek); rozszerzenie jak w liście plików.
static bool safe_name(const char *n)
{
    size_t l = strlen(n);
    if (l == 0 || l >= SD_NAME_MAX || n[0] == '.') return false;
    for (; *n; n++) {
        char c = *n;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return false;
    }
    return true;
}

static esp_err_t put_upload(httpd_req_t *req)
{
    const char *name = req->uri + strlen("/upload/");
    char clean[SD_NAME_MAX];
    // nazwa może być zakodowana procentowo - przyjmujemy tylko bezpieczne znaki, resztę odrzucamy
    strncpy(clean, name, sizeof clean - 1);
    clean[sizeof clean - 1] = 0;
    char *q = strchr(clean, '?');
    if (q) *q = 0;

    if (!safe_name(clean)) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Niedozwolona nazwa (litery, cyfry, . _ -)");
    if (!sdcard_mounted()) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Brak karty SD");
    if (gcode_stream_active()) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_send(req, "Zadanie w toku", HTTPD_RESP_USE_STRLEN);
    }

    char path[SD_NAME_MAX + 16];
    snprintf(path, sizeof path, SD_MOUNT_POINT "/%s", clean);
    FILE *f = fopen(path, "w");
    if (!f) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Nie można utworzyć pliku");

    static char buf[2048];
    int left = req->content_len;
    bool ok = true;
    while (left > 0) {
        int n = httpd_req_recv(req, buf, left < (int)sizeof buf ? left : (int)sizeof buf);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0 || fwrite(buf, 1, n, f) != (size_t)n) { ok = false; break; }
        left -= n;
    }
    fclose(f);
    if (!ok) { remove(path); return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Błąd zapisu"); }
    ESP_LOGI(TAG, "zapisano %s (%d B)", clean, (int)req->content_len);
    return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

void http_upload_start(void)
{
    if (s_srv) return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    if (httpd_start(&s_srv, &cfg) != ESP_OK) { s_srv = NULL; return; }
    httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = get_root};
    httpd_uri_t up = {.uri = "/upload/*", .method = HTTP_PUT, .handler = put_upload};
    httpd_register_uri_handler(s_srv, &root);
    httpd_register_uri_handler(s_srv, &up);
}

void http_upload_stop(void)
{
    if (s_srv) { httpd_stop(s_srv); s_srv = NULL; }
}

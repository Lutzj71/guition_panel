#include "gcode_stream.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "grbl_link.h"
#include "sdcard.h"

#define RX_WINDOW  120      // bajtów w locie (bufor RX grblHAL jest większy; zapas na znaki czasu rzeczywistego)
#define QCAP       40       // maks. linii w locie
#define GS_LINE_MAX   110      // maks. długość linii po oczyszczeniu (bez '\n')

static gs_status_t g;
static SemaphoreHandle_t s_mtx;
static TaskHandle_t s_task;
static volatile bool s_abort;

// kolejka linii w locie (do liczenia znaków i raportowania błędnej linii)
static char s_ql[QCAP][GS_LINE_MAX + 1];
static int s_qh, s_qn, s_inflight;

static void ensure_init(void)
{
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
}

static void on_resp(bool ok, int err, void *ctx)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (s_qn > 0) {
        if (!ok && (g.state == GS_RUNNING || g.state == GS_PAUSED)) {
            g.state = GS_ERROR;
            g.error = err;
            strncpy(g.error_line, s_ql[s_qh], sizeof g.error_line - 1);
        }
        s_inflight -= (int)strlen(s_ql[s_qh]) + 1;
        s_qh = (s_qh + 1) % QCAP;
        s_qn--;
        g.lines_acked++;
    }
    xSemaphoreGive(s_mtx);
    if (s_task) xTaskNotifyGive(s_task);
}

// Usuwa komentarze (; oraz (...)) i białe znaki. Zwraca długość albo -1 gdy za długa.
static int clean_line(const char *in, char *out)
{
    int n = 0, depth = 0;
    for (; *in; in++) {
        char c = *in;
        if (depth) { if (c == ')') depth--; continue; }
        if (c == '(') { depth = 1; continue; }
        if (c == ';') break;
        if (isspace((unsigned char)c)) continue;
        if (n >= GS_LINE_MAX) return -1;
        out[n++] = (char)toupper((unsigned char)c);
    }
    out[n] = 0;
    return n;
}

static void finish(gs_state_t st, const char *msg)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (g.state != GS_ERROR && g.state != GS_ABORTED) g.state = st;
    if (msg) strncpy(g.message, msg, sizeof g.message - 1);
    xSemaphoreGive(s_mtx);
}

static void stream_task(void *arg)
{
    char path[sizeof g.name + 16];
    snprintf(path, sizeof path, SD_MOUNT_POINT "/%s", g.name);
    FILE *f = fopen(path, "r");
    if (!f) { finish(GS_ERROR, "nie można otworzyć pliku"); s_task = NULL; vTaskDelete(NULL); }

    char raw[256], cl[GS_LINE_MAX + 1];
    bool have = false, eof = false;
    int len = 0;

    for (;;) {
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        gs_state_t st = g.state;
        xSemaphoreGive(s_mtx);

        if (s_abort || st == GS_ABORTED) break;
        if (st == GS_ERROR) { ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)); continue; }
        if (st == GS_PAUSED) { ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50)); continue; }

        if (!have && !eof) {
            if (!fgets(raw, sizeof raw, f)) { eof = true; }
            else {
                xSemaphoreTake(s_mtx, portMAX_DELAY);
                g.bytes_done = (uint32_t)ftell(f);
                xSemaphoreGive(s_mtx);
                if (raw[0] == '%') continue;
                len = clean_line(raw, cl);
                if (len < 0) { finish(GS_ERROR, "linia dłuższa niż 110 znaków"); continue; }
                if (len == 0) continue;
                have = true;
            }
        }

        if (eof) {
            xSemaphoreTake(s_mtx, portMAX_DELAY);
            int pending = s_qn;
            xSemaphoreGive(s_mtx);
            grbl_status_t gs;
            grbl_link_get_status(&gs);
            if (pending == 0 && gs.state_id == GRBL_ST_IDLE && !gs.stale) { finish(GS_DONE, NULL); break; }
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
            continue;
        }

        xSemaphoreTake(s_mtx, portMAX_DELAY);
        bool room = (s_inflight + len + 1 <= RX_WINDOW) && (s_qn < QCAP);
        if (room) {
            strcpy(s_ql[(s_qh + s_qn) % QCAP], cl);
            s_qn++;
            s_inflight += len + 1;
            g.lines_sent++;
        }
        xSemaphoreGive(s_mtx);
        if (room) { grbl_send_line(cl); have = false; }
        else ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
    }

    fclose(f);
    s_task = NULL;
    vTaskDelete(NULL);
}

bool gcode_stream_start(const char *name, const char **why)
{
    ensure_init();
    static bool reg;
    if (!reg) { reg = true; grbl_link_add_response_cb(on_resp, NULL); }

    const char *dummy;
    if (!why) why = &dummy;
    grbl_status_t st;
    grbl_link_get_status(&st);

    if (!sdcard_mounted())              { *why = "Brak karty SD"; return false; }
    if (gcode_stream_active() || s_task) { *why = "Zadanie już trwa"; return false; }
    if (st.stale)                       { *why = "Brak łącza z maszyną"; return false; }
    if (st.mpg == 0)                    { *why = "Sterowanie ma komputer (MPG wyłączone)"; return false; }
    if (st.state_id != GRBL_ST_IDLE)    { *why = "Maszyna nie jest w stanie Idle"; return false; }

    char path[sizeof g.name + 16];
    struct stat sb;
    snprintf(path, sizeof path, SD_MOUNT_POINT "/%.*s", SD_PATH_MAX - 1, name);
    if (stat(path, &sb) != 0)           { *why = "Nie znaleziono pliku"; return false; }

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    memset(&g, 0, sizeof g);
    strncpy(g.name, name, sizeof g.name - 1);
    g.bytes_total = (uint32_t)sb.st_size;
    g.state = GS_RUNNING;
    s_qh = s_qn = s_inflight = 0;
    s_abort = false;
    xSemaphoreGive(s_mtx);

    if (xTaskCreate(stream_task, "gcode", 6144, NULL, 5, &s_task) != pdPASS) {
        g.state = GS_IDLE;
        *why = "Brak pamięci na zadanie";
        return false;
    }
    return true;
}

void gcode_stream_pause(void)
{
    ensure_init();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    bool run = g.state == GS_RUNNING;
    if (run) g.state = GS_PAUSED;
    xSemaphoreGive(s_mtx);
    if (run) grbl_send_realtime('!');
}

void gcode_stream_resume(void)
{
    ensure_init();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    bool paused = g.state == GS_PAUSED;
    if (paused) g.state = GS_RUNNING;
    xSemaphoreGive(s_mtx);
    if (paused) { grbl_send_realtime('~'); if (s_task) xTaskNotifyGive(s_task); }
}

void gcode_stream_stop(void)
{
    ensure_init();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    bool act = g.state == GS_RUNNING || g.state == GS_PAUSED || g.state == GS_ERROR;
    if (act) g.state = GS_ABORTED;
    s_abort = true;
    xSemaphoreGive(s_mtx);
    if (s_task) xTaskNotifyGive(s_task);
    // zatrzymanie ruchu, potem miękki reset - wyłącza laser i czyści bufor planera
    grbl_send_realtime('!');
    vTaskDelay(pdMS_TO_TICKS(80));
    grbl_send_realtime(0x18);
}

void gcode_stream_get(gs_status_t *out)
{
    ensure_init();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    *out = g;
    xSemaphoreGive(s_mtx);
}

bool gcode_stream_active(void)
{
    ensure_init();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    bool a = g.state == GS_RUNNING || g.state == GS_PAUSED || g.state == GS_ERROR;
    xSemaphoreGive(s_mtx);
    return a;
}

void gcode_stream_dismiss(void)
{
    ensure_init();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (g.state == GS_DONE || g.state == GS_ABORTED) g.state = GS_IDLE;
    xSemaphoreGive(s_mtx);
}

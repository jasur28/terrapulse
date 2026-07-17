/* SD card CSV logger.
   File structure: YYYYMMDD/STA2_YYMMDDHHMM.csv
   Time[s] = seconds within current minute (0.000 .. 59.995) */

#include "sdcard.h"
#include "adxl345.h"
#include "uart_debug.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

static FATFS  g_fs;
static FIL    g_file;
static uint8_t g_file_open    = 0;
static uint8_t g_cur_minute   = 0xFF; /* invalid sentinel */
static uint8_t g_cur_year     = 0;
static uint8_t g_cur_month    = 0;
static uint8_t g_cur_day      = 0;
static uint8_t g_cur_hour     = 0;
static uint32_t g_sync_counter = 0;
static const char *g_fw_marker = "FW_2026_05_28_B";
#define SYNC_EVERY  20   /* f_sync every 20 samples (~100 ms) */

/* ---- path helpers ---- */

/* Clamp to decimal digit, protecting against DS1302 garbage */
static char d2(uint8_t v, uint8_t max) {
    if (v > max) v = 0;
    return (char)('0' + v);
}

/* buf must be at least 9 bytes.  Produces e.g. "20260107" */
static void make_dir_name(char *buf, const sample_t *s) {
    uint8_t yr = (s->year  > 99u)  ? 0u : s->year;
    uint8_t mo = (s->month < 1u || s->month > 12u) ? 1u : s->month;
    uint8_t dy = (s->day   < 1u || s->day   > 31u) ? 1u : s->day;
    buf[0] = '2'; buf[1] = '0';
    buf[2] = d2(yr / 10u, 9u); buf[3] = d2(yr % 10u, 9u);
    buf[4] = d2(mo / 10u, 9u); buf[5] = d2(mo % 10u, 9u);
    buf[6] = d2(dy / 10u, 9u); buf[7] = d2(dy % 10u, 9u);
    buf[8] = '\0';
}

/* buf must be at least 32 bytes.  Produces e.g. "20260107/STA2_2601071606.csv" */
static void make_file_path(char *buf, const sample_t *s) {
    char    dir[9];
    uint8_t yr = (s->year  > 99u)  ? 0u : s->year;
    uint8_t mo = (s->month < 1u || s->month > 12u) ? 1u : s->month;
    uint8_t dy = (s->day   < 1u || s->day   > 31u) ? 1u : s->day;
    uint8_t hr = (s->hour  > 23u)  ? 0u : s->hour;
    uint8_t mn = (s->minute > 59u) ? 0u : s->minute;

    make_dir_name(dir, s);
    memcpy(buf, dir, 8);
    buf[8]  = '/';
    buf[9]  = 'S'; buf[10] = 'T'; buf[11] = 'A'; buf[12] = '2'; buf[13] = '_';
    buf[14] = d2(yr / 10u, 9u); buf[15] = d2(yr % 10u, 9u);
    buf[16] = d2(mo / 10u, 9u); buf[17] = d2(mo % 10u, 9u);
    buf[18] = d2(dy / 10u, 9u); buf[19] = d2(dy % 10u, 9u);
    buf[20] = d2(hr / 10u, 9u); buf[21] = d2(hr % 10u, 9u);
    buf[22] = d2(mn / 10u, 9u); buf[23] = d2(mn % 10u, 9u);
    buf[24] = '.'; buf[25] = 'c'; buf[26] = 's'; buf[27] = 'v';
    buf[28] = '\0';
}

/* ---- open/close helpers ---- */

static void close_current_file(void) {
    if (g_file_open) {
        f_sync(&g_file);
        f_close(&g_file);
        g_file_open = 0;
    }
}

static int open_new_file(const sample_t *s) {
    char     path[32];
    char     dir[9];
    char     header[128];
    FILINFO  fi;
    FRESULT  res;
    UINT     bw;
    int      n;

    make_dir_name(dir, s);
    res = f_stat(dir, &fi);
    if (res == FR_OK) {
        if ((fi.fattrib & AM_DIR) == 0u) {
            uart_puts("[SD] path exists but is not a dir\r\n");
            return -1;
        }
    } else {
        res = f_mkdir(dir);
        if (res != FR_OK && res != FR_EXIST) {
            uart_printf("[SD] mkdir failed %d\r\n", (int)res);
            return -1;
        }
    }

    make_file_path(path, s);
    res = f_open(&g_file, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        uart_printf("[SD] open failed %d: %s\r\n", (int)res, path);
        return -1;
    }

    n = snprintf(header, sizeof(header),
                 "DateTime,20%02u/%02u/%02u %02u:%02u:%02u\r\n"
                 "Firmware,%s\r\n"
                 "\r\n"
                 "Time[s],X,Y,Z\r\n",
                 (unsigned)s->year, (unsigned)s->month, (unsigned)s->day,
                 (unsigned)s->hour, (unsigned)s->minute, (unsigned)s->second,
                 g_fw_marker);
    if (n <= 0) { f_close(&g_file); return -1; }
    res = f_write(&g_file, header, (UINT)n, &bw);
    if (res != FR_OK) { f_close(&g_file); return -1; }
    f_sync(&g_file); /* flush dir entry + FAT immediately so power-off can't corrupt them */

    g_file_open  = 1;
    g_cur_minute = s->minute;
    g_cur_year   = s->year;
    g_cur_month  = s->month;
    g_cur_day    = s->day;
    g_cur_hour   = s->hour;
    g_sync_counter = 0;

    uart_printf("[SD] new file: %s\r\n", path);
    return 0;
}

/* ---- public API ---- */

int sdcard_init(void) {
    FRESULT res = f_mount(&g_fs, "", 1);
    if (res != FR_OK) {
        uart_printf("[SD] mount failed %d\r\n", (int)res);
        return -1;
    }
    uart_printf("[SD] ready\r\n");
    return 0;
}

int sdcard_start_logging(const sample_t *first_sample) {
    close_current_file();
    return open_new_file(first_sample);
}

/* Called from main loop to drain the ring buffer into CSV. */
void sdcard_process(ring_buf_t *rb) {
    sample_t s;
    char     line[72];
    char     sx[14], sy[14], sz[14];
    UINT     bw;
    uint32_t t_whole, t_frac;

    while (rb_pop(rb, &s) == 0) {
        /* Minute boundary → close current file, open new one */
        int new_minute = (s.minute != g_cur_minute)
                      || (s.hour   != g_cur_hour)
                      || (s.day    != g_cur_day)
                      || (s.month  != g_cur_month)
                      || (s.year   != g_cur_year);

        if (!g_file_open || new_minute) {
            close_current_file();
            if (open_new_file(&s) != 0)
                return; /* SD error, give up this batch */
        }

        /* Format time: seconds.milliseconds within current minute */
        t_whole = s.time_ms / 1000;
        t_frac  = s.time_ms % 1000;

        format_gal_value(sx, raw_to_gal_x10000(s.raw_x));
        format_gal_value(sy, raw_to_gal_x10000(s.raw_y));
        format_gal_value(sz, raw_to_gal_x10000(s.raw_z));

        /* Build line manually (no printf) */
        int i = 0;
        /* time whole part */
        if (t_whole >= 10) { line[i++] = '0' + (int)(t_whole / 10); }
        line[i++] = '0' + (int)(t_whole % 10);
        line[i++] = '.';
        line[i++] = '0' + (int)(t_frac / 100);
        line[i++] = '0' + (int)((t_frac / 10) % 10);
        line[i++] = '0' + (int)(t_frac % 10);
        line[i++] = ',';
        /* X */
        int slen = (int)strlen(sx);
        memcpy(&line[i], sx, (size_t)slen); i += slen;
        line[i++] = ',';
        /* Y */
        slen = (int)strlen(sy);
        memcpy(&line[i], sy, (size_t)slen); i += slen;
        line[i++] = ',';
        /* Z */
        slen = (int)strlen(sz);
        memcpy(&line[i], sz, (size_t)slen); i += slen;
        line[i++] = '\r';
        line[i++] = '\n';

        if (f_write(&g_file, line, (UINT)i, &bw) != FR_OK)
            return;

        if (++g_sync_counter >= SYNC_EVERY) {
            f_sync(&g_file);
            g_sync_counter = 0;
        }
    }
}

void sdcard_stop_logging(void) {
    close_current_file();
}

int sdcard_write_sample_now(const sample_t *s) {
    ring_buf_t dummy_rb;
    rb_init(&dummy_rb);
    rb_push(&dummy_rb, s);
    sdcard_process(&dummy_rb);
    return 0;
}

/* STM32F103 Seismic Logger — CA-01NP format
 * 200 Hz, YYYYMMDD/STA2_YYMMDDHHMM.CSV, gal units
 * MCU: STM32F103C8T6  Crystal: 8 MHz  → 72 MHz via PLL
 */
#include "stm32f10x.h"
#include "ff.h"
#include "uart_debug.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ── Tuning ─────────────────────────────────────────────────── */
#define SAMPLE_HZ    200U
#define SAMPLE_MS    (1000U / SAMPLE_HZ)   /* 5 ms */
#define SYNC_EVERY   200U                  /* f_sync every 1 s */
#define RETRY_MS     2000U

/* ── Types ──────────────────────────────────────────────────── */
typedef struct { uint16_t year; uint8_t mon, day, hh, mm, ss; } dt_t;
typedef struct { uint8_t ss; uint16_t ms; int16_t x, y, z; }    smp_t;

/* ── Globals ────────────────────────────────────────────────── */
static volatile uint32_t g_tick;

static FATFS g_fs;
static FIL   g_fil;
static uint8_t  g_sd_open;
static FRESULT  g_fr;
static uint8_t  g_log_hh = 0xFFU, g_log_mm = 0xFFU;
static uint32_t g_sync_n;

static uint8_t g_adxl_ok;
static uint32_t g_stream_seq;

/* ═══════════════════════════════════════════════════════════════
   SysTick  (1 ms)
   ════════════════════════════════════════════════════════════ */
void SysTick_Handler(void) { g_tick++; }
static inline uint32_t ms(void) { return g_tick; }
static void wait_ms(uint32_t n)
{
    uint32_t end = ms() + n;
    while ((int32_t)(ms() - end) < 0) {}
}

/* ═══════════════════════════════════════════════════════════════
   PC13 LED  — active LOW (Blue Pill built-in)
   Blink count at startup = FRESULT error:
     3  FR_NOT_READY   → SPI / wiring bad
    13  FR_NO_FILESYSTEM → format card as FAT32!
   ════════════════════════════════════════════════════════════ */
#define LED_ON()   (GPIOC->BRR  = (1U << 13))
#define LED_OFF()  (GPIOC->BSRR = (1U << 13))

static void led_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_1;
    LED_OFF();
}

static void led_blink(uint8_t n)
{
    for (uint8_t i = 0; i < n; i++) {
        LED_ON(); wait_ms(300U); LED_OFF(); wait_ms(300U);
    }
    wait_ms(1000U);
}

/* ═══════════════════════════════════════════════════════════════
   DS1302 RTC   PB12=CLK  PB13=IO  PB14=RST
   ════════════════════════════════════════════════════════════ */
#define RTC_CLK  12U
#define RTC_IO   13U
#define RTC_RST  14U
#define RCLK_H() (GPIOB->BSRR = 1U << RTC_CLK)
#define RCLK_L() (GPIOB->BRR  = 1U << RTC_CLK)
#define RRST_H() (GPIOB->BSRR = 1U << RTC_RST)
#define RRST_L() (GPIOB->BRR  = 1U << RTC_RST)
#define RIO_H()  (GPIOB->BSRR = 1U << RTC_IO)
#define RIO_L()  (GPIOB->BRR  = 1U << RTC_IO)
#define RIO_R()  ((GPIOB->IDR >> RTC_IO) & 1U)

static void rnop(void) { __NOP(); __NOP(); __NOP(); __NOP(); }

static void rio_out(void)
{
    GPIOB->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOB->CRH |=  GPIO_CRH_MODE13_1;
}
static void rio_in(void)
{
    GPIOB->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOB->CRH |=  GPIO_CRH_CNF13_0; /* floating input */
}

static void rtc_send(uint8_t v)
{
    uint8_t i;
    rio_out();
    for (i = 0; i < 8U; i++, v >>= 1) {
        RCLK_L(); (v & 1U) ? RIO_H() : RIO_L(); rnop(); RCLK_H(); rnop();
    }
}
static uint8_t rtc_recv(void)
{
    uint8_t v = 0U, i;
    rio_in();
    for (i = 0; i < 8U; i++) {
        RCLK_L(); rnop(); if (RIO_R()) v |= (1U << i); RCLK_H(); rnop();
    }
    return v;
}

static uint8_t bcd2d(uint8_t b) { return (b >> 4) * 10U + (b & 0xFU); }
static uint8_t d2bcd(uint8_t d) { return ((d / 10U) << 4) | (d % 10U); }

static void rtc_hw_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    GPIOB->CRH &= ~(GPIO_CRH_MODE12|GPIO_CRH_CNF12|
                    GPIO_CRH_MODE13|GPIO_CRH_CNF13|
                    GPIO_CRH_MODE14|GPIO_CRH_CNF14);
    GPIOB->CRH |= GPIO_CRH_MODE12_1 | GPIO_CRH_MODE14_1;
    rio_out();
    RCLK_L(); RRST_L(); RIO_L();
    /* clear write-protect */
    RRST_H(); rnop(); rtc_send(0x8EU); rtc_send(0x00U); RRST_L(); rnop();
}

static uint8_t rtc_valid(const dt_t *d)
{
    if (!d) return 0U;
    if (d->year < 2024U || d->year > 2099U) return 0U;
    if (d->mon  <  1U   || d->mon  >  12U) return 0U;
    if (d->day  <  1U   || d->day  >  31U) return 0U;
    if (d->hh > 23U || d->mm > 59U || d->ss > 59U) return 0U;
    return 1U;
}

static int8_t dt_cmp(const dt_t *a, const dt_t *b)
{
    if (a->year != b->year) return (a->year > b->year) ? 1 : -1;
    if (a->mon  != b->mon)  return (a->mon  > b->mon)  ? 1 : -1;
    if (a->day  != b->day)  return (a->day  > b->day)  ? 1 : -1;
    if (a->hh   != b->hh)   return (a->hh   > b->hh)   ? 1 : -1;
    if (a->mm   != b->mm)   return (a->mm   > b->mm)   ? 1 : -1;
    if (a->ss   != b->ss)   return (a->ss   > b->ss)   ? 1 : -1;
    return 0;
}

static uint8_t rtc_read(dt_t *d)
{
    uint8_t b[8], i;
    RRST_H(); rnop();
    rtc_send(0xBFU); /* burst read */
    for (i = 0; i < 8U; i++) b[i] = rtc_recv();
    RRST_L(); rnop();
    if (b[0] & 0x80U) return 0U;
    d->ss  = bcd2d(b[0] & 0x7FU);
    d->mm  = bcd2d(b[1] & 0x7FU);
    d->hh  = bcd2d(b[2] & 0x3FU);
    d->day = bcd2d(b[3] & 0x3FU);
    d->mon = bcd2d(b[4] & 0x1FU);
    d->year = 2000U + (uint16_t)bcd2d(b[6]);
    return rtc_valid(d);
}

static void rtc_write(const dt_t *d)
{
    RRST_H(); rnop(); rtc_send(0x8EU); rtc_send(0x00U); RRST_L(); rnop();
    RRST_H(); rnop();
    rtc_send(0xBEU); /* burst write */
    rtc_send(d2bcd(d->ss));
    rtc_send(d2bcd(d->mm));
    rtc_send(d2bcd(d->hh));
    rtc_send(d2bcd(d->day));
    rtc_send(d2bcd(d->mon));
    rtc_send(1U);
    rtc_send(d2bcd((uint8_t)(d->year % 100U)));
    rtc_send(0x80U);
    RRST_L(); rnop();
}

static void rtc_tick(dt_t *d)
{
    static const uint8_t dom[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t maxd;
    d->ss++;
    if (d->ss < 60U) return;
    d->ss = 0U; d->mm++;
    if (d->mm < 60U) return;
    d->mm = 0U; d->hh++;
    if (d->hh < 24U) return;
    d->hh = 0U; d->day++;
    maxd = dom[d->mon - 1U];
    if (d->mon == 2U) {
        if (d->year % 400U == 0U || (d->year % 4U == 0U && d->year % 100U != 0U))
            maxd = 29U;
    }
    if (d->day <= maxd) return;
    d->day = 1U; d->mon++;
    if (d->mon <= 12U) return;
    d->mon = 1U; d->year++;
}

/* ── build-time RTC fallback ─────────────────────────────── */
static uint8_t mon_from(const char *s)
{
    if (s[0]=='J'&&s[1]=='a') return  1U;
    if (s[0]=='F')             return  2U;
    if (s[0]=='M'&&s[2]=='r') return  3U;
    if (s[0]=='A'&&s[1]=='p') return  4U;
    if (s[0]=='M'&&s[2]=='y') return  5U;
    if (s[0]=='J'&&s[2]=='n') return  6U;
    if (s[0]=='J'&&s[2]=='l') return  7U;
    if (s[0]=='A')             return  8U;
    if (s[0]=='S')             return  9U;
    if (s[0]=='O')             return 10U;
    if (s[0]=='N')             return 11U;
    return 12U;
}
static void dt_from_build(dt_t *d)
{
    d->year = (uint16_t)((__DATE__[7]-'0')*1000 + (__DATE__[8]-'0')*100 +
                         (__DATE__[9]-'0')*10   +  (__DATE__[10]-'0'));
    d->mon  = mon_from(__DATE__);
    d->day  = (uint8_t)(((__DATE__[4]==' ')?0:(__DATE__[4]-'0'))*10 + (__DATE__[5]-'0'));
    d->hh   = (uint8_t)((__TIME__[0]-'0')*10 + (__TIME__[1]-'0'));
    d->mm   = (uint8_t)((__TIME__[3]-'0')*10 + (__TIME__[4]-'0'));
    d->ss   = (uint8_t)((__TIME__[6]-'0')*10 + (__TIME__[7]-'0'));
}

/* ═══════════════════════════════════════════════════════════════
   ADXL345  I2C1  PB6=SCL  PB7=SDA
   ════════════════════════════════════════════════════════════ */
#define AW   0xA6U
#define AR   0xA7U
#define ITO  80000U

static uint8_t i2c_start(uint8_t addr)
{
    uint32_t t = ITO;
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB))    { if (!--t) return 0U; }
    (void)I2C1->SR1; I2C1->DR = addr; t = ITO;
    while (!(I2C1->SR1 & (I2C_SR1_ADDR|I2C_SR1_AF))) { if (!--t) return 0U; }
    if (I2C1->SR1 & I2C_SR1_AF) { I2C1->SR1 &= (uint16_t)~I2C_SR1_AF; return 0U; }
    (void)I2C1->SR1; (void)I2C1->SR2;
    return 1U;
}
static uint8_t i2c_wr(uint8_t d)
{
    uint32_t t = ITO;
    while (!(I2C1->SR1 & I2C_SR1_TXE)) { if (!--t) return 0U; }
    I2C1->DR = d; t = ITO;
    while (!(I2C1->SR1 & I2C_SR1_BTF)) {
        if (I2C1->SR1 & I2C_SR1_AF) { I2C1->SR1 &= (uint16_t)~I2C_SR1_AF; return 0U; }
        if (!--t) return 0U;
    }
    return 1U;
}
static uint8_t i2c_rd(uint8_t *d, uint8_t ack)
{
    uint32_t t = ITO;
    if (ack) I2C1->CR1 |=  I2C_CR1_ACK;
    else   { I2C1->CR1 &= (uint16_t)~I2C_CR1_ACK; I2C1->CR1 |= I2C_CR1_STOP; }
    while (!(I2C1->SR1 & I2C_SR1_RXNE)) { if (!--t) return 0U; }
    *d = (uint8_t)I2C1->DR;
    return 1U;
}
static void i2c_stop(void) { I2C1->CR1 |= I2C_CR1_STOP; }

static void adxl_bus_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    GPIOB->CRL &= ~(GPIO_CRL_MODE6|GPIO_CRL_CNF6|GPIO_CRL_MODE7|GPIO_CRL_CNF7);
    GPIOB->CRL |= (GPIO_CRL_MODE6_1|GPIO_CRL_MODE6_0|GPIO_CRL_CNF6_1|GPIO_CRL_CNF6_0);
    GPIOB->CRL |= (GPIO_CRL_MODE7_1|GPIO_CRL_MODE7_0|GPIO_CRL_CNF7_1|GPIO_CRL_CNF7_0);
    I2C1->CR1 = I2C_CR1_SWRST; I2C1->CR1 = 0U;
    I2C1->CR2 = 36U; I2C1->CCR = 180U; I2C1->TRISE = 37U;
    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;
}
static uint8_t adxl_wreg(uint8_t r, uint8_t v)
{
    if (!i2c_start(AW)) return 0U;
    if (!i2c_wr(r)) { i2c_stop(); return 0U; }
    if (!i2c_wr(v)) { i2c_stop(); return 0U; }
    i2c_stop(); return 1U;
}
static uint8_t adxl_init(void)
{
    uint8_t id = 0U;
    if (!i2c_start(AW)) return 0U;
    if (!i2c_wr(0x00U)) { i2c_stop(); return 0U; }
    if (!i2c_start(AR)) return 0U;
    if (!i2c_rd(&id, 0U)) return 0U;
    if (id != 0xE5U) return 0U;
    if (!adxl_wreg(0x2DU, 0x00U)) return 0U; /* standby */
    if (!adxl_wreg(0x2CU, 0x0BU)) return 0U; /* 200 Hz  */
    if (!adxl_wreg(0x31U, 0x0BU)) return 0U; /* ±16g FR */
    if (!adxl_wreg(0x2DU, 0x08U)) return 0U; /* measure */
    return 1U;
}
static uint8_t adxl_read(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t b[6], i;
    if (!i2c_start(AW)) return 0U;
    if (!i2c_wr(0x32U)) { i2c_stop(); return 0U; }
    if (!i2c_start(AR)) return 0U;
    for (i = 0U; i < 5U; i++) if (!i2c_rd(&b[i], 1U)) return 0U;
    if (!i2c_rd(&b[5], 0U)) return 0U;
    *ax = (int16_t)(((uint16_t)b[1] << 8) | b[0]);
    *ay = (int16_t)(((uint16_t)b[3] << 8) | b[2]);
    *az = (int16_t)(((uint16_t)b[5] << 8) | b[4]);
    return 1U;
}

/* ═══════════════════════════════════════════════════════════════
   Conversion: ADXL raw → gal×10000 (1 LSB = 3.9 mg in FULL_RES)
   ════════════════════════════════════════════════════════════ */
static int32_t raw2gal(int16_t r)
{
    int64_t v = (int64_t)r * 980665LL * 39LL;
    return (int32_t)((v + (v >= 0 ? 500LL : -500LL)) / 1000LL);
}
static void fmt_gal(char *buf, size_t sz, int32_t v)
{
    uint32_t m = (v < 0) ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    if (v < 0)
        (void)snprintf(buf, sz, "-%lu.%04lu",
                       (unsigned long)(m / 10000U), (unsigned long)(m % 10000U));
    else
        (void)snprintf(buf, sz, "%lu.%04lu",
                       (unsigned long)(m / 10000U), (unsigned long)(m % 10000U));
}

/* ═══════════════════════════════════════════════════════════════
   SD / FatFs   Format: 0:/YYYYMMDD/STA2_YYMMDDHHMM.CSV
   ════════════════════════════════════════════════════════════ */
/* UART real-time stream, miniSEED-like packet framing.
   Fixed 43-byte little-endian packet:
   magic "MS", version, type, length, sequence, sample rate,
   RTC date/time, millisecond, stream id, X/Y/Z in gal*10000, CRC16. */
#define STREAM_PACKET_LEN 43U

static uint16_t crc16_ccitt(const uint8_t *p, uint16_t n)
{
    uint16_t crc = 0xFFFFU;
    uint8_t bit;

    while (n--) {
        crc ^= (uint16_t)(*p++) << 8;
        for (bit = 0U; bit < 8U; bit++) {
            if (crc & 0x8000U) crc = (uint16_t)((crc << 1) ^ 0x1021U);
            else               crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static void pkt_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void pkt_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void stream_send_sample(const dt_t *d, const smp_t *s)
{
    static const uint8_t sid[8] = {'S','T','A','2','X','Y','Z',0};
    uint8_t pkt[STREAM_PACKET_LEN];
    uint16_t crc;
    uint8_t i;

    pkt[0] = 'M';
    pkt[1] = 'S';
    pkt[2] = 1U;
    pkt[3] = 1U;
    pkt_u16(&pkt[4], STREAM_PACKET_LEN);
    pkt_u32(&pkt[6], g_stream_seq++);
    pkt_u16(&pkt[10], SAMPLE_HZ);
    pkt_u16(&pkt[12], d->year);
    pkt[14] = d->mon;
    pkt[15] = d->day;
    pkt[16] = d->hh;
    pkt[17] = d->mm;
    pkt[18] = s->ss;
    pkt_u16(&pkt[19], s->ms);
    memcpy(&pkt[21], sid, sizeof(sid));
    pkt_u32(&pkt[29], (uint32_t)raw2gal(s->x));
    pkt_u32(&pkt[33], (uint32_t)raw2gal(s->y));
    pkt_u32(&pkt[37], (uint32_t)raw2gal(s->z));

    crc = crc16_ccitt(pkt, STREAM_PACKET_LEN - 2U);
    pkt_u16(&pkt[41], crc);

    for (i = 0U; i < STREAM_PACKET_LEN; i++) {
        uart_putchar((char)pkt[i]);
    }
}

static void sd_close(void)
{
    if (!g_sd_open) return;
    (void)f_sync(&g_fil);
    (void)f_close(&g_fil);
    g_sd_open = 0U;
    g_log_hh = g_log_mm = 0xFFU;
}

static uint8_t sd_open(const dt_t *d)
{
    static const char hdr[] = "Time[s],X,Y,Z\r\n";
    char dir[20], path[48];
    FILINFO fi;
    UINT bw;
    int  n;

    /* mount */
    g_fr = f_mount(&g_fs, "", 1U);
    if (g_fr != FR_OK) return 0U;

    /* create directory only if it does not already exist */
    n = snprintf(dir, sizeof(dir), "%04u%02u%02u",
                 (unsigned)d->year, (unsigned)d->mon, (unsigned)d->day);
    if (n <= 0) return 0U;
    g_fr = f_stat(dir, &fi);
    if (g_fr == FR_OK) {
        if ((fi.fattrib & AM_DIR) == 0U) return 0U;
    } else {
        g_fr = f_mkdir(dir);
        if (g_fr != FR_OK && g_fr != FR_EXIST) return 0U;
    }

    /* open file STA2_YYMMDDHHMM.CSV */
    n = snprintf(path, sizeof(path), "%s/STA2_%02u%02u%02u%02u%02u.CSV",
                 dir,
                 (unsigned)(d->year % 100U), (unsigned)d->mon, (unsigned)d->day,
                 (unsigned)d->hh, (unsigned)d->mm);
    if (n <= 0) return 0U;
    g_fr = f_open(&g_fil, path, FA_OPEN_ALWAYS | FA_WRITE);
    if (g_fr != FR_OK) return 0U;

    if (f_size(&g_fil) == 0U) {
        (void)f_write(&g_fil, hdr, (UINT)(sizeof(hdr) - 1U), &bw);
    } else {
        (void)f_lseek(&g_fil, f_size(&g_fil));
    }
    (void)f_sync(&g_fil);

    g_sd_open = 1U;
    g_sync_n  = 0U;
    g_log_hh  = d->hh;
    g_log_mm  = d->mm;
    return 1U;
}

static uint8_t sd_ensure(const dt_t *d)
{
    if (!g_sd_open) return 0U;
    if (g_log_hh == d->hh && g_log_mm == d->mm) return 1U;
    sd_close();
    return sd_open(d);
}

static void sd_write(const smp_t *s)
{
    char line[80], xb[16], yb[16], zb[16];
    UINT bw;
    int  n;

    if (!g_sd_open) return;

    fmt_gal(xb, sizeof(xb), raw2gal(s->x));
    fmt_gal(yb, sizeof(yb), raw2gal(s->y));
    fmt_gal(zb, sizeof(zb), raw2gal(s->z));

    n = snprintf(line, sizeof(line), "%u.%03u,%s,%s,%s\r\n",
                 (unsigned)s->ss, (unsigned)s->ms, xb, yb, zb);
    if (n <= 0) return;

    g_fr = f_write(&g_fil, line, (UINT)n, &bw);
    if (g_fr != FR_OK || bw != (UINT)n) {
        (void)f_close(&g_fil); g_sd_open = 0U; return;
    }
    if (++g_sync_n >= SYNC_EVERY) {
        g_sync_n = 0U; (void)f_sync(&g_fil);
    }
}

/* ═══════════════════════════════════════════════════════════════
   main
   ════════════════════════════════════════════════════════════ */
int main(void)
{
    dt_t     dt;
    dt_t     build_dt;
    smp_t    s;
    int16_t  ax, ay, az;
    uint32_t t_now, next_smp, next_retry;
    uint16_t ms_acc;

    SystemInit();
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000U);

    uart_init();
    led_init();
    rtc_hw_init();
    adxl_bus_init();

    /* wait up to 2 s for ADXL */
    {
        uint32_t end = ms() + 2000U;
        g_adxl_ok = 0U;
        while (!g_adxl_ok && (int32_t)(ms() - end) < 0)
            g_adxl_ok = adxl_init();
    }

    /* RTC — use build time if DS1302 not set */
    dt_from_build(&build_dt);
    if (!rtc_read(&dt)) {
        dt = build_dt;
        rtc_write(&dt);
    } else if (dt_cmp(&dt, &build_dt) < 0) {
        dt = build_dt;
        rtc_write(&dt);
    }

    /* open SD card */
    g_sd_open = 0U;
    if (!sd_open(&dt)) {
        /* blink error code: 3=not ready, 13=no FAT32 filesystem */
        uint8_t code = ((uint8_t)g_fr >= 1U && (uint8_t)g_fr <= 15U)
                       ? (uint8_t)g_fr : 15U;
        led_blink(code);
    }

    ms_acc     = 0U;
    next_smp   = ms();
    next_retry = ms() + RETRY_MS;

    while (1) {
        t_now = ms();

        /* ── 200 Hz sample loop ─────────────────────────────── */
        while ((t_now - next_smp) >= SAMPLE_MS) {
            next_smp += SAMPLE_MS;

            if (g_adxl_ok && adxl_read(&ax, &ay, &az)) {
                s.ss = dt.ss;
                s.ms = ms_acc;
                s.x  = ax; s.y = ay; s.z = az;
                stream_send_sample(&dt, &s);
                if (sd_ensure(&dt)) sd_write(&s);
            } else {
                g_adxl_ok = adxl_init();
            }

            ms_acc += (uint16_t)SAMPLE_MS;
            while (ms_acc >= 1000U) { ms_acc -= 1000U; rtc_tick(&dt); }
        }

        /* ── SD retry (once per RETRY_MS when file is closed) ─ */
        if (!g_sd_open && (int32_t)(t_now - next_retry) >= 0) {
            next_retry = t_now + RETRY_MS;
            (void)sd_open(&dt);
        }

        /* ── LED heartbeat ─────────────────────────────────── */
        {
            static uint32_t nxt_led = 0U, led_off = 0U;
            if ((int32_t)(t_now - led_off) >= 0) LED_OFF();
            if ((int32_t)(t_now - nxt_led) >= 0) {
                LED_ON();
                nxt_led = t_now + 1000U;
                led_off = t_now + (g_sd_open ? 50U : 500U);
            }
        }
    }
}

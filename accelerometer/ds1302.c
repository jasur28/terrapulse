/* DS1302 RTC - 3-wire bit-bang, register-only (PB12=CLK, PB13=IO, PB14=RST) */
#include "ds1302.h"
#include "stm32f10x.h"

/* Bit operations via BSRR (set) and BRR (reset) */
#define DS_CLK_H()  (GPIOB->BSRR = (1UL << 12))
#define DS_CLK_L()  (GPIOB->BRR  = (1UL << 12))
#define DS_RST_H()  (GPIOB->BSRR = (1UL << 14))
#define DS_RST_L()  (GPIOB->BRR  = (1UL << 14))
#define DS_IO_H()   (GPIOB->BSRR = (1UL << 13))
#define DS_IO_L()   (GPIOB->BRR  = (1UL << 13))
#define DS_IO_IN()  ((GPIOB->IDR >> 13) & 1u)

/* CRH bit positions for PB13 (IO pin, changes direction) */
#define PB13_CRH_SHIFT  20u
#define PB13_CFG_OUT    0x3u   /* 50 MHz PP output */
#define PB13_CFG_INP    0x8u   /* pull-up input */

static void delay_us(uint32_t us) {
    volatile uint32_t n = us * 18u;
    while (n--) {}
}

static void io_dir_out(void) {
    GPIOB->CRH = (GPIOB->CRH & ~(0xFUL << PB13_CRH_SHIFT))
               | (PB13_CFG_OUT << PB13_CRH_SHIFT);
}

static void io_dir_in(void) {
    GPIOB->CRH = (GPIOB->CRH & ~(0xFUL << PB13_CRH_SHIFT))
               | (PB13_CFG_INP << PB13_CRH_SHIFT);
    GPIOB->ODR |= (1UL << 13);
}

static uint8_t bcd2bin(uint8_t b) { return (uint8_t)((b >> 4) * 10u + (b & 0x0Fu)); }
static uint8_t bin2bcd(uint8_t b) { return (uint8_t)(((b / 10u) << 4) | (b % 10u)); }

static void ds_write_byte(uint8_t byte) {
    uint8_t i;
    io_dir_out();
    for (i = 0; i < 8u; i++) {
        if (byte & 1u) DS_IO_H(); else DS_IO_L();
        delay_us(1);
        DS_CLK_H();
        delay_us(1);
        DS_CLK_L();
        delay_us(1);
        byte >>= 1;
    }
}

static uint8_t ds_read_byte(void) {
    uint8_t i, byte = 0;
    io_dir_in();
    delay_us(2);
    for (i = 0; i < 8u; i++) {
        byte >>= 1;
        if (DS_IO_IN()) byte |= 0x80u;
        DS_CLK_H();
        delay_us(1);
        DS_CLK_L();
        delay_us(1);
    }
    return byte;
}

static void ds_begin(void) {
    DS_RST_L();
    DS_CLK_L();
    delay_us(4);
    DS_RST_H();
    delay_us(4);
}

static void ds_end(void) {
    DS_RST_L();
    delay_us(4);
}

static void ds_write_reg(uint8_t cmd, uint8_t val) {
    ds_begin();
    ds_write_byte(cmd);
    ds_write_byte(val);
    ds_end();
}

void rtc_init(void) {
    /* Enable GPIOB clock (APB2 bit3) */
    RCC->APB2ENR |= (1UL << 3);

    /* PB12=CLK, PB13=IO, PB14=RST -- 50 MHz PP output */
    GPIOB->CRH = (GPIOB->CRH & 0xF000FFFFul) | (0x333ul << 16);

    DS_RST_L();
    DS_CLK_L();
    DS_IO_L();

    /* Disable write-protect register.
       Do not force any fixed calendar date here. */
    ds_write_reg(0x8Eu, 0x00u);
}

void rtc_read_datetime(rtc_datetime_t *dt) {
    uint8_t i, buf[8];
    ds_begin();
    ds_write_byte(0xBFu);
    for (i = 0; i < 8u; i++) buf[i] = ds_read_byte();
    ds_end();

    dt->second = bcd2bin(buf[0] & 0x7Fu);
    dt->minute = bcd2bin(buf[1] & 0x7Fu);
    dt->hour   = bcd2bin(buf[2] & 0x3Fu);
    dt->day    = bcd2bin(buf[3] & 0x3Fu);
    dt->month  = bcd2bin(buf[4] & 0x1Fu);
    dt->year   = bcd2bin(buf[6]);
}

void rtc_write_datetime(const rtc_datetime_t *dt) {
    uint8_t i;
    uint8_t buf[8] = {
        bin2bcd(dt->second),
        bin2bcd(dt->minute),
        bin2bcd(dt->hour),
        bin2bcd(dt->day),
        bin2bcd(dt->month),
        0x01u,
        bin2bcd(dt->year),
        0x00u
    };

    ds_write_reg(0x8Eu, 0x00u);
    ds_begin();
    ds_write_byte(0xBEu);
    for (i = 0; i < 8u; i++) ds_write_byte(buf[i]);
    ds_end();
}

void rtc_increment_second(rtc_datetime_t *dt) {
    static const uint8_t dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t max;

    if (++dt->second < 60u) return;
    dt->second = 0;
    if (++dt->minute < 60u) return;
    dt->minute = 0;
    if (++dt->hour < 24u) return;
    dt->hour = 0;

    max = dim[dt->month - 1u];
    if (dt->month == 2u && (dt->year % 4u) == 0u) max = 29u;
    if (++dt->day <= max) return;
    dt->day = 1;
    if (++dt->month <= 12u) return;
    dt->month = 1;
    if (++dt->year > 99u) dt->year = 0;
}

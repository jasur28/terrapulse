/* ADXL345 over I2C1 (PB6=SCL, PB7=SDA, 400kHz) — register-only driver */
#include "adxl345.h"
#include "stm32f10x.h"

#define ADXL_ADDR_W  (0x53u << 1)       /* 0xA6 */
#define ADXL_ADDR_R  ((0x53u << 1) | 1) /* 0xA7 */
#define I2C_TIMEOUT  20000u

/* I2C1 SR1 bits */
#define SR1_SB    (1UL<<0)
#define SR1_ADDR  (1UL<<1)
#define SR1_BTF   (1UL<<2)
#define SR1_RXNE  (1UL<<6)
#define SR1_TXE   (1UL<<7)
/* I2C1 CR1 bits */
#define CR1_PE    (1UL<<0)
#define CR1_STOP  (1UL<<9)
#define CR1_START (1UL<<8)
#define CR1_ACK   (1UL<<10)

static int wait_sr1(uint32_t flag) {
    uint32_t t = I2C_TIMEOUT;
    while (!(I2C1->SR1 & flag)) {
        if (--t == 0) return -1;
    }
    return 0;
}

static void i2c_stop(void) {
    I2C1->CR1 |= CR1_STOP;
    /* small wait for stop condition */
    volatile uint32_t d = 200; while (d--) {}
}

static int i2c_write_reg(uint8_t reg, uint8_t val) {
    I2C1->CR1 |= CR1_START;
    if (wait_sr1(SR1_SB))   { i2c_stop(); return -1; }
    I2C1->DR = ADXL_ADDR_W;
    if (wait_sr1(SR1_ADDR)) { i2c_stop(); return -1; }
    (void)I2C1->SR1; (void)I2C1->SR2; /* clear ADDR */

    if (wait_sr1(SR1_TXE))  { i2c_stop(); return -1; }
    I2C1->DR = reg;
    if (wait_sr1(SR1_TXE))  { i2c_stop(); return -1; }
    I2C1->DR = val;
    if (wait_sr1(SR1_BTF))  { i2c_stop(); return -1; }
    i2c_stop();
    return 0;
}

static int i2c_read_burst(uint8_t reg, uint8_t *buf, uint8_t len) {
    uint8_t i;

    /* Write phase — set register pointer */
    I2C1->CR1 |= CR1_START;
    if (wait_sr1(SR1_SB))   { i2c_stop(); return -1; }
    I2C1->DR = ADXL_ADDR_W;
    if (wait_sr1(SR1_ADDR)) { i2c_stop(); return -1; }
    (void)I2C1->SR1; (void)I2C1->SR2;
    if (wait_sr1(SR1_TXE))  { i2c_stop(); return -1; }
    I2C1->DR = reg;
    if (wait_sr1(SR1_BTF))  { i2c_stop(); return -1; }

    /* Repeated START — read phase */
    I2C1->CR1 |= CR1_ACK | CR1_START;
    if (wait_sr1(SR1_SB))   { i2c_stop(); return -1; }
    I2C1->DR = ADXL_ADDR_R;
    if (wait_sr1(SR1_ADDR)) { i2c_stop(); return -1; }
    (void)I2C1->SR1; (void)I2C1->SR2;

    for (i = 0; i < len; i++) {
        if (i == (uint8_t)(len - 1)) {
            I2C1->CR1 &= ~CR1_ACK;
            I2C1->CR1 |=  CR1_STOP;
        }
        if (wait_sr1(SR1_RXNE)) { return -1; }
        buf[i] = (uint8_t)I2C1->DR;
    }
    return 0;
}

int adxl345_init(void) {
    /* Enable GPIOB (bit3) and I2C1 (bit21) clocks */
    RCC->APB2ENR |= (1UL << 3);
    RCC->APB1ENR |= (1UL << 21);

    /* PB6=SCL, PB7=SDA: AF Open-Drain 50 MHz (config = 0xF each)
       CRL bits[27:24]=PB6, bits[31:28]=PB7 */
    GPIOB->CRL = (GPIOB->CRL & 0x00FFFFFFul) | 0xFF000000ul;

    /* Software reset I2C1 to recover from any stuck BUSY */
    RCC->APB1RSTR |= (1UL << 21);
    RCC->APB1RSTR &= ~(1UL << 21);

    I2C1->CR1 = 0;
    I2C1->CR2 = 36;               /* FREQ=36 (PCLK1=36 MHz) */
    I2C1->CCR = (1UL<<15) | 30;  /* FS=1, CCR=30 → 400 kHz */
    I2C1->TRISE = 11;             /* floor(300 ns × 36 MHz) + 1 */
    I2C1->CR1 = CR1_PE;

    if (i2c_write_reg(0x2C, 0x0B)) return -1; /* BW_RATE: 200 Hz */
    if (i2c_write_reg(0x31, 0x0B)) return -1; /* DATA_FORMAT: ±16g full-res */
    if (i2c_write_reg(0x2D, 0x08)) return -1; /* POWER_CTL: measure */
    return 0;
}

int adxl345_read_raw(int16_t *x, int16_t *y, int16_t *z) {
    uint8_t buf[6];
    if (i2c_read_burst(0x32, buf, 6)) return -1;
    *x = (int16_t)((uint16_t)((uint16_t)buf[1] << 8) | buf[0]);
    *y = (int16_t)((uint16_t)((uint16_t)buf[3] << 8) | buf[2]);
    *z = (int16_t)((uint16_t)((uint16_t)buf[5] << 8) | buf[4]);
    return 0;
}

/* ±16g full-res: 3.9 mg/LSB = 0.0039 g/LSB
   gal_x10000 = raw × 0.0039 × 980.665 × 10000
              = raw × 980665 × 39 / 1 000 000 */
int32_t raw_to_gal_x10000(int16_t raw) {
    return (int32_t)(((int64_t)raw * 980665LL * 39LL) / 1000000LL);
}

void format_gal_value(char *buf, int32_t v) {
    int32_t whole, frac;
    int     neg = 0, i = 0;

    if (v < 0) { neg = 1; v = -v; }
    whole = v / 10000;
    frac  = v % 10000;

    if (neg) buf[i++] = '-';
    if      (whole >= 10000) { buf[i++] = '0' + (int)(whole/10000); whole %= 10000;
                                buf[i++] = '0' + (int)(whole/1000);  whole %= 1000;
                                buf[i++] = '0' + (int)(whole/100);   whole %= 100;
                                buf[i++] = '0' + (int)(whole/10);    whole %= 10;  }
    else if (whole >= 1000)  { buf[i++] = '0' + (int)(whole/1000);  whole %= 1000;
                                buf[i++] = '0' + (int)(whole/100);   whole %= 100;
                                buf[i++] = '0' + (int)(whole/10);    whole %= 10;  }
    else if (whole >= 100)   { buf[i++] = '0' + (int)(whole/100);   whole %= 100;
                                buf[i++] = '0' + (int)(whole/10);    whole %= 10;  }
    else if (whole >= 10)    { buf[i++] = '0' + (int)(whole/10);    whole %= 10;  }
    buf[i++] = '0' + (int)whole;
    buf[i++] = '.';
    buf[i++] = '0' + (int)(frac / 1000); frac %= 1000;
    buf[i++] = '0' + (int)(frac / 100);  frac %= 100;
    buf[i++] = '0' + (int)(frac / 10);   frac %= 10;
    buf[i++] = '0' + (int)frac;
    buf[i]   = '\0';
}

#ifndef ADXL345_H
#define ADXL345_H

#include <stdint.h>

/* I2C1: SCL=PB6, SDA=PB7, addr=0x53 */

int     adxl345_init(void);
int     adxl345_read_raw(int16_t *x, int16_t *y, int16_t *z);

/* Returns gal * 10000 (fixed-point).  ±16g full-res: 3.9 mg/LSB */
int32_t raw_to_gal_x10000(int16_t raw);

/* Formats gal_x10000 into buf as "-0.3489" (4 decimal places) */
void    format_gal_value(char *buf, int32_t gal_x10000);

#endif

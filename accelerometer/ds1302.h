#ifndef DS1302_H
#define DS1302_H

#include <stdint.h>

/* 3-wire: CLK=PB12, IO=PB13, RST=PB14 */

typedef struct {
    uint8_t year;   /* 0..99 (2-digit) */
    uint8_t month;  /* 1..12 */
    uint8_t day;    /* 1..31 */
    uint8_t hour;   /* 0..23 */
    uint8_t minute; /* 0..59 */
    uint8_t second; /* 0..59 */
} rtc_datetime_t;

void rtc_init(void);
void rtc_read_datetime(rtc_datetime_t *dt);
void rtc_write_datetime(const rtc_datetime_t *dt);

/* Increment second (for simulation / test without hardware) */
void rtc_increment_second(rtc_datetime_t *dt);

#endif

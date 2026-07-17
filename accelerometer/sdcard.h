#ifndef SDCARD_H
#define SDCARD_H

#include "ring_buffer.h"

/* SPI1: CS=PA4, SCK=PA5, MISO=PA6, MOSI=PA7
   File format: YYYYMMDD/STA2_YYMMDDHHMM.csv
   1 file = 1 calendar minute, Time[s] = seconds within minute */

int  sdcard_init(void);
int  sdcard_start_logging(const sample_t *first_sample);
void sdcard_process(ring_buf_t *rb);  /* call from main loop */
void sdcard_stop_logging(void);

/* Write a single sample immediately (blocking, no buffer) */
int  sdcard_write_sample_now(const sample_t *s);

#endif

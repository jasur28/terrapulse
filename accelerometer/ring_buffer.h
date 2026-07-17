#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

typedef struct {
    uint32_t time_ms;   /* ms within current minute (0..59999) */
    int16_t  raw_x;
    int16_t  raw_y;
    int16_t  raw_z;
    uint8_t  year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  _pad[2];
} sample_t; /* 20 bytes */

#define RING_BUF_SIZE  512

typedef struct {
    sample_t         buf[RING_BUF_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint32_t dropped;
} ring_buf_t;

void     rb_init(ring_buf_t *rb);
int      rb_push(ring_buf_t *rb, const sample_t *s);  /* call from ISR */
int      rb_pop(ring_buf_t *rb, sample_t *s);
uint16_t rb_count(const ring_buf_t *rb);

#endif

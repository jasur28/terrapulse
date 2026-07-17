#include "ring_buffer.h"

void rb_init(ring_buf_t *rb) {
    rb->head    = 0;
    rb->tail    = 0;
    rb->dropped = 0;
}

/* Called from ISR — no disable needed if head is written last (single producer) */
int rb_push(ring_buf_t *rb, const sample_t *s) {
    uint16_t next = (rb->head + 1) % RING_BUF_SIZE;
    if (next == rb->tail) {
        rb->dropped++;
        return -1;
    }
    rb->buf[rb->head] = *s;
    rb->head = next;
    return 0;
}

int rb_pop(ring_buf_t *rb, sample_t *s) {
    if (rb->head == rb->tail)
        return -1;
    *s = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUF_SIZE;
    return 0;
}

uint16_t rb_count(const ring_buf_t *rb) {
    return (uint16_t)((rb->head - rb->tail + RING_BUF_SIZE) % RING_BUF_SIZE);
}

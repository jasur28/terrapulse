#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#include <stdint.h>

/* UART1: TX=PA9, RX=PA10, 460800 baud */

void uart_init(void);
void uart_putchar(char c);
void uart_puts(const char *s);
void uart_printf(const char *fmt, ...);
int uart_getchar_nonblock(void);

#endif

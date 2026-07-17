/* UART1 debug output — register-only (PA9=TX, PA10=RX, 115200 baud, 72 MHz PCLK2) */
#include "uart_debug.h"
#include "stm32f10x.h"
#include <stdarg.h>

void uart_init(void) {
    /* Enable GPIOA (bit2) and USART1 (bit14) clocks on APB2 */
    RCC->APB2ENR |= (1UL << 2) | (1UL << 14);

    /* PA9  = AF Push-Pull 50 MHz (TX): CRH bits[7:4]  = 0xB
       PA10 = Floating input     (RX): CRH bits[11:8]  = 0x4 */
    GPIOA->CRH = (GPIOA->CRH & 0xFFFFF00FUL)
               | (0xBUL << 4)
               | (0x4UL << 8);

    /* BRR for 460800 baud at 72 MHz PCLK2: USARTDIV*16 = 156.25 -> 0x09C */
    USART1->BRR = 0x09C;
    /* CR1: UE=1, TE=1, RE=1 */
    USART1->CR1 = (1UL << 13) | (1UL << 3) | (1UL << 2);
}

void uart_putchar(char c) {
    while (!(USART1->SR & (1UL << 7))) {} /* wait TXE */
    USART1->DR = (uint32_t)(uint8_t)c;
}

int uart_getchar_nonblock(void) {
    if (!(USART1->SR & (1UL << 5))) { /* RXNE */
        return -1;
    }
    return (int)(USART1->DR & 0xFFu);
}

void uart_puts(const char *s) {
    while (*s) uart_putchar(*s++);
}

/* Minimal printf: %s %c %d %u %lu %02d %04d */
void uart_printf(const char *fmt, ...) {
    va_list ap;
    char    tmp[12];
    int     i, len;
    int     width;
    char    pad;

    va_start(ap, fmt);
    while (*fmt) {
        if (*fmt != '%') { uart_putchar(*fmt++); continue; }
        fmt++;

        pad   = ' ';
        width = 0;
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt++ - '0'); }

        if (*fmt == 'l') fmt++; /* skip 'l' prefix */

        switch (*fmt++) {
        case 's':
            uart_puts(va_arg(ap, const char *));
            break;

        case 'c':
            uart_putchar((char)va_arg(ap, int));
            break;

        case 'd':
        case 'i': {
            int32_t  v = va_arg(ap, int32_t);
            uint32_t u;
            int      neg = (v < 0);
            u = neg ? (uint32_t)(-(v + 1)) + 1u : (uint32_t)v;
            len = 0;
            if (u == 0) { tmp[len++] = '0'; }
            else { while (u) { tmp[len++] = '0' + (char)(u % 10u); u /= 10u; } }
            for (i = 0; i < width - len - (neg ? 1 : 0); i++) uart_putchar(pad);
            if (neg) uart_putchar('-');
            for (i = len - 1; i >= 0; i--) uart_putchar(tmp[i]);
            break;
        }

        case 'u': {
            uint32_t u = va_arg(ap, uint32_t);
            len = 0;
            if (u == 0) { tmp[len++] = '0'; }
            else { while (u) { tmp[len++] = '0' + (char)(u % 10u); u /= 10u; } }
            for (i = 0; i < width - len; i++) uart_putchar(pad);
            for (i = len - 1; i >= 0; i--) uart_putchar(tmp[i]);
            break;
        }

        default:
            uart_putchar(*(fmt - 1));
            break;
        }
    }
    va_end(ap);
}

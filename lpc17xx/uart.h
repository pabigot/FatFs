#ifndef _UART_DEF
#define _UART_DEF

#include "LPC1700.h"

void uart_init (void);
int uart_test (void);
void uart_putc (uint8_t);
uint8_t uart_getc (void);

#endif

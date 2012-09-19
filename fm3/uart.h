#ifndef UART_DEFINED
#define UART_DEFINED
#include <stdint.h>

void uart_init (void);
int uart_test (void);
void uart_putc (uint8_t);
uint8_t uart_getc (void);

#endif

#ifndef _UART_DEF
#define _UART_DEF

#include "LPC2300.h"

#define __kbhit()	uart_test()
#define __getch()	uart_getc()

void uart_init (void);
int uart_test (void);
void uart_putc (uint8_t);
uint8_t uart_getc (void);

#endif


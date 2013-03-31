#ifndef _COMMFUNC
#define _COMMFUNC

#include "integer.h"

void uart_init (DWORD bps);
int uart_test (void);
void uart_putc (BYTE d);
BYTE uart_getc (void);

#endif


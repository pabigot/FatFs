#ifndef _SCIF2
#define _SCIF2

#include "integer.h"

void scif2_init (DWORD bps);
int scif2_test (void);
void scif2_putc (BYTE d);
BYTE scif2_getc (void);

#endif


#ifndef SCIF_DEFINED
#define SCIF_DEFINED

#include <stdint.h>

void scif_init (uint32_t bps);
int scif_test (void);
void scif_putc (uint8_t dat);
uint8_t scif_getc (void);

#endif



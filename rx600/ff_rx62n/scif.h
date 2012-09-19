#ifndef _SCIF
#define _SCIF

#include <stdint.h>

void scif_init (uint32_t bps);
int scif_test (void);
void scif_put (uint8_t dat);
uint8_t scif_get (void);

#endif


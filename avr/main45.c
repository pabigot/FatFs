/*---------------------------------------------------------------*/
/* PFF Test Program for ATtiny45                                 */
/*---------------------------------------------------------------*/
/* _USE_DIR, _USE_LSEEK and _USE_WRITE should be 0 to fit code
/  in the 4KB of program memory.
*/

#include <avr/io.h>
#include "pff.h"

FUSES = {0xE2, 0xDD, 0xFF};	/* Fuses: low, high, extended. */

int main (void)
{
	FATFS fs;
	UINT br;

	/* Initialize GPIO ports */
	PORTB = 0b101011;	/* u z H L H u */
	DDRB =  0b001110;

	/* Open a text file and type it */
	if (pf_mount(&fs) == FR_OK &&
		pf_open("hello.txt") == FR_OK) {
		do {
			pf_read(0, 16384, &br);	/* Direct output to the console */
		} while (br == 16384);
	}

	for (;;) ;
}


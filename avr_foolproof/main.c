/*----------------------------------------------------------------------*/
/* Foolproof FatFs sample project for AVR              (C)ChaN, 2013    */
/*----------------------------------------------------------------------*/

#include <avr/io.h>	/* Device specific declarations */
#include "ff.h"		/* Declarations of FatFs API */

FATFS FatFs;		/* FatFs work area needed for each volume */
FIL Fil;			/* File object needed for each open file */


int main (void)
{
	UINT bw;


	f_mount(&FatFs, "", 0);		/* Give a work area to the default drive */

	if (f_open(&Fil, "newfile.txt", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {	/* Create a file */

		f_write(&Fil, "It works!\r\n", 11, &bw);	/* Write data to the file */

		f_close(&Fil);								/* Close the file */

		if (bw == 11) {		/* Lights green LED if data written well */
			DDRB |= 0x10; PORTB |= 0x10;	/* Set PB4 high */
		}
	}

	for (;;) ;
}



/*---------------------------------------------------------*/
/* User Provided RTC Function called by FatFs module       */

DWORD get_fattime (void)
{
	/* Returns current time packed into a DWORD variable */
	return	  ((DWORD)(2013 - 1980) << 25)	/* Year 2013 */
			| ((DWORD)7 << 21)				/* Month 7 */
			| ((DWORD)28 << 16)				/* Mday 28 */
			| ((DWORD)0 << 11)				/* Hour 0 */
			| ((DWORD)0 << 5)				/* Min 0 */
			| ((DWORD)0 >> 1);				/* Sec 0 */
}


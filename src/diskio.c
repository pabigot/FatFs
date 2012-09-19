/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for Petit FatFs (C)ChaN, 2009      */
/*-----------------------------------------------------------------------*/

#include "diskio.h"



/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (void)
{
	DSTATUS stat;

	// Put your code here

	return stat;
}



/*-----------------------------------------------------------------------*/
/* Read Partial Sector                                                   */
/*-----------------------------------------------------------------------*/

DRESULT disk_readp (
	void* dest,			/* Pointer to the destination object */
	DWORD sector,		/* Sector number (LBA) */
	WORD sofs,			/* Offset in the sector */
	WORD count			/* Byte count (bit15:destination) */
)
{
	DRESULT res;

	// Put your code here

	return res;
}


/*-----------------------------------------------------------------------
/  Low level disk interface modlue include file   (C)ChaN, 2013
/-----------------------------------------------------------------------*/

#ifndef _DISKIO_DEFINED
#define _DISKIO_DEFINED

#ifdef __cplusplus
extern "C" {
#endif


#include "integer.h"


/* Status of Disk Functions */
typedef BYTE	DSTATUS;

/* Results of Disk Functions */
typedef enum {
	RES_OK = 0,		/* 0: Successful */
	RES_ERROR,		/* 1: R/W Error */
	RES_WRPRT,		/* 2: Write Protected */
	RES_NOTRDY,		/* 3: Not Ready */
	RES_PARERR		/* 4: Invalid Parameter */
} DRESULT;


/*---------------------------------------*/
/* Prototypes for disk control functions */

DSTATUS disk_initialize (BYTE pdrv);
DSTATUS disk_status (BYTE pdrv);
DRESULT disk_read (BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
DRESULT disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff);


/* Disk Status Bits (DSTATUS) */

#define STA_NOINIT		0x01	/* Drive not initialized */
#define STA_NODISK		0x02	/* No medium in the drive */
#define STA_PROTECT		0x04	/* Write protected */


/* Command code for disk_ioctrl fucntion */

/* Generic ioctl command (defined by FatFs) */
#define CTRL_SYNC			0	/* Flush disk cache (for write functions) */
#define GET_SECTOR_COUNT	1	/* Get media size (for only f_mkfs()) */
#define GET_SECTOR_SIZE		2	/* Get sector size (for multiple sector size (_MAX_SS >= 1024)) */
#define GET_BLOCK_SIZE		3	/* Get erase block size (for only f_mkfs()) */
#define CTRL_ERASE_SECTOR	4	/* Force erased a block of sectors (for only _USE_ERASE) */

/* Generic ioctl command */
#define CTRL_FORMAT			5	/* Create physical format on the media */
#define CTRL_POWER_IDLE		6	/* Put the device idle state */
#define CTRL_POWER_OFF		7	/* Put the device off state */
#define CTRL_LOCK			8	/* Lock media removal */
#define CTRL_UNLOCK			9	/* Unlock media removal */
#define CTRL_EJECT			10	/* Eject media */

/* MMC/SDC specific ioctl command */
#define MMC_GET_TYPE		50	/* Get card type */
#define MMC_GET_CSD			51	/* Get CSD */
#define MMC_GET_CID			52	/* Get CID */
#define MMC_GET_OCR			53	/* Get OCR */
#define MMC_GET_SDSTAT		54	/* Get SD status */

/* ATA/CF specific ioctl command */
#define ATA_GET_REV			60	/* Get F/W revision */
#define ATA_GET_MODEL		61	/* Get model name */
#define ATA_GET_SN			62	/* Get serial number */


/* Card type flags (MMC_GET_TYPE) */
#define CT_MMC		0x01		/* MMC ver 3 */
#define CT_SD1		0x02		/* SD ver 1 */
#define CT_SD2		0x04		/* SD ver 2 */
#define CT_SDC		(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK	0x08		/* Block addressing */


/*---------------------------------------------*/
/* Prototypes for each physical disk functions */

#define DN_MCI		0	/* Physical drive number for MCI */
#define DN_NAND		1	/* Physical drive number for NAND flash */

DSTATUS NAND_initialize (void);
DSTATUS NAND_status (void);
DRESULT NAND_read (BYTE*, DWORD, UINT);
DRESULT NAND_write (const BYTE*, DWORD, UINT);
DRESULT NAND_ioctl (BYTE, void*);

DSTATUS MCI_initialize (void);
DSTATUS MCI_status (void);
DRESULT MCI_read (BYTE*, DWORD, UINT);
DRESULT MCI_write (const BYTE*, DWORD, UINT);
DRESULT MCI_ioctl (BYTE, void*);
void MCI_timerproc (void);

/* These functions are defined in asmfunc.S */
void Copy_al2un (BYTE *dst, const DWORD *src, int count);	/* Copy aligned to unaligned. */
void Copy_un2al (DWORD *dst, const BYTE *src, int count);	/* Copy unaligned to aligned. */


#endif

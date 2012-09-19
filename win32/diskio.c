/*-----------------------------------------------------------------------*/
/* Low level disk control module for Win32              (C)ChaN, 2007    */
/*-----------------------------------------------------------------------*/

#include <stdio.h>
#include "diskio.h"
#include <windows.h>
#include <winioctl.h>



/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/


#define	IHV	INVALID_HANDLE_VALUE
#define	BUFSIZE 65536UL

typedef struct {
	DSTATUS	status;
	WORD sz_sector;
	DWORD n_sectors;
	HANDLE h_drive;
} STAT;

static
HANDLE hMutex, hTmrThread;

static volatile
STAT Stat[10];


static
DWORD TmrThreadID;

static
BYTE *Buffer;	/* Poiter to the data transfer buffer */


/*-----------------------------------------------------------------------*/
/* Timer Functions                                                       */
/*-----------------------------------------------------------------------*/


DWORD WINAPI tmr_thread (LPVOID parms)
{
	DWORD dw;
	int drv;


	for (;;) {
		for (drv = 0; drv < 10; drv++) {
			Sleep(1);
			if (Stat[drv].h_drive == IHV || Stat[drv].status & STA_NOINIT || WaitForSingleObject(hMutex, 100) != WAIT_OBJECT_0) continue;

			if (!DeviceIoControl(Stat[drv].h_drive, IOCTL_STORAGE_CHECK_VERIFY, NULL, 0, NULL, 0, &dw, NULL))
				Stat[drv].status |= STA_NOINIT;
			ReleaseMutex(hMutex);
			Sleep(100);
		}
	}
}



BOOL get_status (volatile STAT *stat) {
	DISK_GEOMETRY parms;
	PARTITION_INFORMATION part;
	DWORD dw;
	HANDLE h = stat->h_drive;


	if (h == IHV
		|| !DeviceIoControl(h, IOCTL_STORAGE_CHECK_VERIFY, NULL, 0, NULL, 0, &dw, NULL)
		|| !DeviceIoControl(h, IOCTL_DISK_GET_PARTITION_INFO, NULL, 0, &part, sizeof(part), &dw, NULL)
		|| !DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &parms, sizeof(parms), &dw, NULL)) {
		stat->status = STA_NOINIT;
		return FALSE;
	}
	stat->sz_sector = (WORD)parms.BytesPerSector;
	stat->n_sectors = (DWORD)(part.PartitionLength.QuadPart / parms.BytesPerSector);
	stat->status = DeviceIoControl(h, IOCTL_DISK_IS_WRITABLE, NULL, 0, NULL, 0, &dw, NULL) ? 0 : STA_PROTECT;
	return TRUE;
}




/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


BOOL assign_drives (int argc, char *argv[])
{
	int pd, n, nd;
	char str[30];
	HANDLE h;


	Buffer = VirtualAlloc(NULL, BUFSIZE, MEM_COMMIT, PAGE_READWRITE);
	if (!Buffer) return FALSE;

	hMutex = CreateMutex(0, FALSE, NULL);
	if (hMutex == IHV) return FALSE;

	pd = nd = 0;
	while (pd < 10 && pd < argc - 1) {
		n = atoi(argv[pd + 1]);
		if (!n) return FALSE;
		sprintf(str, "\\\\.\\PhysicalDrive%u", n);
		printf("%s ==> Disk#%u on FatFs", str, pd);
		h = CreateFile(str, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
		Stat[pd].h_drive = h;
		if (h == IHV) {
			Stat[pd].status = STA_NOINIT;
			printf(" (Not Available)\n");
		} else {
			if (get_status(&Stat[pd])) {
				printf(" (%u Sectors, %u Bytes/Sector)\n", Stat[pd].n_sectors, Stat[pd].sz_sector);
			} else {
				printf(" (Not Ready)\n");
			}
			nd++;
		}
		pd++;
	}

	hTmrThread = CreateThread(NULL, 0, tmr_thread, 0, 0, &TmrThreadID);
	if (hTmrThread == IHV) nd = 0;

	return nd ? TRUE : FALSE;
}





/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE drv		/* Physical drive nmuber */
)
{
	DSTATUS sta;


	if (WaitForSingleObject(hMutex, 5000) != WAIT_OBJECT_0) return STA_NOINIT;

	if (drv >= 10) {
		sta = STA_NOINIT;
	} else {
		get_status(&Stat[drv]);
		sta = Stat[drv].status;
	}

	ReleaseMutex(hMutex);
	return sta;
}



/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE drv		/* Physical drive nmuber (0) */
)
{
	DSTATUS sta;


	if (drv >= 10) {
		sta = STA_NOINIT;
	} else {
		sta = Stat[drv].status;
	}

	return sta;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE drv,			/* Physical drive nmuber (0) */
	BYTE *buff,			/* Pointer to the data buffer to store read data */
	DWORD sector,		/* Start sector number (LBA) */
	BYTE count			/* Sector count (1..255) */
)
{
	DWORD nc, rnc;
	LARGE_INTEGER ofs;
	DSTATUS res;


	if (drv >= 10 || Stat[drv].status & STA_NOINIT || WaitForSingleObject(hMutex, 3000) != WAIT_OBJECT_0) return RES_NOTRDY;

	nc = (DWORD)count * Stat[drv].sz_sector;
	if (nc > BUFSIZE) {
		res = RES_PARERR;
	} else {
		ofs.QuadPart = (LONGLONG)sector * Stat[drv].sz_sector;
		if (SetFilePointer(Stat[drv].h_drive, ofs.LowPart, &ofs.HighPart, FILE_BEGIN) != ofs.LowPart) {
			res = RES_ERROR;
		} else {
			if (!ReadFile(Stat[drv].h_drive, Buffer, nc, &rnc, NULL) || nc != rnc) {
				res = RES_ERROR;
			} else {
				memcpy(buff, Buffer, nc);
				res = RES_OK;
			}
		}
	}

	ReleaseMutex(hMutex);
	return res;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _READONLY == 0
DRESULT disk_write (
	BYTE drv,			/* Physical drive nmuber (0) */
	const BYTE *buff,	/* Pointer to the data to be written */
	DWORD sector,		/* Start sector number (LBA) */
	BYTE count			/* Sector count (1..255) */
)
{
	DWORD nc, rnc;
	LARGE_INTEGER ofs;
	DRESULT res;


	if (drv >= 10 || Stat[drv].status & STA_NOINIT || WaitForSingleObject(hMutex, 3000) != WAIT_OBJECT_0) return RES_NOTRDY;

	res = RES_OK;
	if (Stat[drv].status & STA_PROTECT) {
		res = RES_WRPRT;
	} else {
		nc = (DWORD)count * Stat[drv].sz_sector;
		if (nc > BUFSIZE)
			res = RES_PARERR;
	}

	if (res == RES_OK) {
		ofs.QuadPart = (LONGLONG)sector * Stat[drv].sz_sector;
		if (SetFilePointer(Stat[drv].h_drive, ofs.LowPart, &ofs.HighPart, FILE_BEGIN) != ofs.LowPart) {
			res = RES_ERROR;
		} else {
			memcpy(Buffer, buff, nc);
			if (!WriteFile(Stat[drv].h_drive, Buffer, nc, &rnc, NULL) || nc != rnc)
				res = RES_ERROR;
		}
	}

	ReleaseMutex(hMutex);
	return res;
}
#endif /* _READONLY */



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE drv,		/* Physical drive nmuber (0) */
	BYTE ctrl,		/* Control code */
	void *buff		/* Buffer to send/receive data block */
)
{
	DRESULT res = RES_PARERR;


	if (drv >= 10 || (Stat[drv].status & STA_NOINIT)) return RES_NOTRDY;

	switch (ctrl) {
	case CTRL_SYNC:
		res = RES_OK;
		break;

	case GET_SECTOR_COUNT:
		*(DWORD*)buff = Stat[drv].n_sectors;
		res = RES_OK;
		break;

	case GET_SECTOR_SIZE:
		*(WORD*)buff = Stat[drv].sz_sector;
		res = RES_OK;
		break;

	case GET_BLOCK_SIZE:
		*(DWORD*)buff = 128;
		res = RES_OK;
		break;
	}

	return res;
}





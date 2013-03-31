/*------------------------------------------------------------------------/
/  The Main Development Bench of FatFs Module
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2013, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/


#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <locale.h>
#include "diskio.h"
#include "ff.h"

int assign_drives (void);


#ifdef UNICODE
#if !_LFN_UNICODE
#error Configuration mismatch. _LFN_UNICODE must be 1.
#endif
#else
#if _LFN_UNICODE
#error Configuration mismatch. _LFN_UNICODE must be 0.
#endif
#endif



#if _MULTI_PARTITION	/* Volume - partition resolution table (Example) */
PARTITION VolToPart[] = {
	{0, 0},	/* "0:" <== Disk# 0, auto detect */
	{1, 0},	/* "1:" <== Disk# 1, auto detect */
	{2, 0},	/* "2:" <== Disk# 2, auto detect */
	{3, 1},	/* "3:" <== Disk# 3, 1st partition */
	{3, 2},	/* "4:" <== Disk# 3, 2nd partition */
	{3, 3},	/* "5:" <== Disk# 3, 3rd partition */
	{4, 0},	/* "6:" <== Disk# 4, auto detect */
	{5, 0}	/* "7:" <== Disk# 5, auto detect */
};
#endif


/*---------------------------------------------------------*/
/* Work Area                                               */
/*---------------------------------------------------------*/


LONGLONG AccSize;			/* Work register for scan_files() */
WORD AccFiles, AccDirs;
FILINFO Finfo;
#if _USE_LFN
TCHAR Lfname[256];
#endif

TCHAR Line[300];			/* Console input/output buffer */
HANDLE hCon, hKey;

FATFS FatFs[_VOLUMES];		/* File system object for logical drive */
BYTE Buff[262144];			/* Working buffer */

#if _USE_FASTSEEK
DWORD SeekTbl[16];			/* Link map table for fast seek feature */
#endif


/*---------------------------------------------------------*/
/* User Provided RTC Function for FatFs module             */
/*---------------------------------------------------------*/
/* This is a real time clock service to be called from     */
/* FatFs module. Any valid time must be returned even if   */
/* the system does not support an RTC.                     */
/* This function is not required in read-only cfg.         */

DWORD get_fattime (void)
{
	SYSTEMTIME tm;

	/* Get local time */
	GetLocalTime(&tm);

	/* Pack date and time into a DWORD variable */
	return 	  ((DWORD)(tm.wYear - 1980) << 25)
			| ((DWORD)tm.wMonth << 21)
			| ((DWORD)tm.wDay << 16)
			| (WORD)(tm.wHour << 11)
			| (WORD)(tm.wMinute << 5)
			| (WORD)(tm.wSecond >> 1);
}



/*--------------------------------------------------------------------------*/
/* Monitor                                                                  */

/*----------------------------------------------*/
/* Get a value of the string                    */
/*----------------------------------------------*/
/*	"123 -5   0x3ff 0b1111 0377  w "
	    ^                           1st call returns 123 and next ptr
	       ^                        2nd call returns -5 and next ptr
                   ^                3rd call returns 1023 and next ptr
                          ^         4th call returns 15 and next ptr
                               ^    5th call returns 255 and next ptr
                                  ^ 6th call fails and returns 0
*/

int xatoi (			/* 0:Failed, 1:Successful */
	TCHAR **str,	/* Pointer to pointer to the string */
	long *res		/* Pointer to a valiable to store the value */
)
{
	unsigned long val;
	unsigned char r, s = 0;
	TCHAR c;


	*res = 0;
	while ((c = **str) == ' ') (*str)++;	/* Skip leading spaces */

	if (c == '-') {		/* negative? */
		s = 1;
		c = *(++(*str));
	}

	if (c == '0') {
		c = *(++(*str));
		switch (c) {
		case 'x':		/* hexdecimal */
			r = 16; c = *(++(*str));
			break;
		case 'b':		/* binary */
			r = 2; c = *(++(*str));
			break;
		default:
			if (c <= ' ') return 1;	/* single zero */
			if (c < '0' || c > '9') return 0;	/* invalid char */
			r = 8;		/* octal */
		}
	} else {
		if (c < '0' || c > '9') return 0;	/* EOL or invalid char */
		r = 10;			/* decimal */
	}

	val = 0;
	while (c > ' ') {
		if (c >= 'a') c -= 0x20;
		c -= '0';
		if (c >= 17) {
			c -= 7;
			if (c <= 9) return 0;	/* invalid char */
		}
		if (c >= r) return 0;		/* invalid char for current radix */
		val = val * r + c;
		c = *(++(*str));
	}
	if (s) val = 0 - val;			/* apply sign if needed */

	*res = val;
	return 1;
}


/*----------------------------------------------*/
/* Dump a block of byte array                   */

void put_dump (
	const unsigned char* buff,	/* Pointer to the byte array to be dumped */
	unsigned long addr,			/* Heading address value */
	int cnt						/* Number of bytes to be dumped */
)
{
	int i;


	_tprintf(_T("%08lX:"), addr);

	for (i = 0; i < cnt; i++)
		_tprintf(_T(" %02X"), buff[i]);

	_puttchar(' ');
	for (i = 0; i < cnt; i++)
		_puttchar((TCHAR)((buff[i] >= ' ' && buff[i] <= '~') ? buff[i] : '.'));

	_puttchar('\n');
}



FRESULT scan_files (
	TCHAR* path		/* Pointer to the path name working buffer */
)
{
	DIR dirs;
	FRESULT res;
	int i;
	TCHAR *fn;


	if ((res = f_opendir(&dirs, path)) == FR_OK) {
		i = _tcslen(path);
		while (((res = f_readdir(&dirs, &Finfo)) == FR_OK) && Finfo.fname[0]) {
			if (_FS_RPATH && Finfo.fname[0] == '.') continue;
#if _USE_LFN
			fn = *Finfo.lfname ? Finfo.lfname : Finfo.fname;
#else
			fn = Finfo.fname;
#endif
			if (Finfo.fattrib & AM_DIR) {
				AccDirs++;
				*(path+i) = '/'; _tcscpy(path+i+1, fn);
				res = scan_files(path);
				*(path+i) = '\0';
				if (res != FR_OK) break;
			} else {
//				_tprintf(_T("%s/%s\n"), path, fn);
				AccFiles++;
				AccSize += Finfo.fsize;
			}
		}
	}

	return res;
}



void put_rc (FRESULT rc)
{
	const TCHAR *p =
		_T("OK\0DISK_ERR\0INT_ERR\0NOT_READY\0NO_FILE\0NO_PATH\0INVALID_NAME\0")
		_T("DENIED\0EXIST\0INVALID_OBJECT\0WRITE_PROTECTED\0INVALID_DRIVE\0")
		_T("NOT_ENABLED\0NO_FILE_SYSTEM\0MKFS_ABORTED\0TIMEOUT\0LOCKED\0")
		_T("NOT_ENOUGH_CORE\0TOO_MANY_OPEN_FILES\0");
	FRESULT i;

	for (i = 0; i != rc && *p; i++) {
		while(*p++) ;
	}
	_tprintf(_T("rc=%u FR_%s\n"), (UINT)rc, p);
}



const char HelpStr[] = {
		_T("[Disk contorls]\n")
		_T(" di <pd#> - Initialize disk\n")
		_T(" dd [<pd#> <sect>] - Dump a secrtor\n")
		_T(" ds <pd#> - Show disk status\n")
		_T("[Buffer contorls]\n")
		_T(" bd <ofs> - Dump working buffer\n")
		_T(" be <ofs> [<data>] ... - Edit working buffer\n")
		_T(" br <pd#> <sect> <count> - Read disk into working buffer\n")
		_T(" bw <pd#> <sect> <count> - Write working buffer into disk\n")
		_T(" bf <val> - Fill working buffer\n")
		_T("[File system contorls]\n")
		_T(" fi <ld#> - Force initialized the volume\n")
		_T(" fs [<path>] - Show volume status\n")
		_T(" fl [<path>] - Show a directory\n")
		_T(" fo <mode> <file> - Open a file\n")
		_T(" fc - Close the file\n")
		_T(" fe <ofs> - Move fp in normal seek\n")
		_T(" fE <ofs> - Move fp in fast seek or Create link table\n")
		_T(" fd <len> - Read and dump the file\n")
		_T(" fr <len> - Read the file\n")
		_T(" fw <len> <val> - Write to the file\n")
		_T(" fn <object name> <new name> - Rename an object\n")
		_T(" fu <object name> - Unlink an object\n")
		_T(" fv - Truncate the file at current fp\n")
		_T(" fk <dir name> - Create a directory\n")
		_T(" fa <atrr> <mask> <object name> - Change object attribute\n")
		_T(" ft <year> <month> <day> <hour> <min> <sec> <object name> - Change timestamp of an object\n")
		_T(" fx <src file> <dst file> - Copy a file\n")
		_T(" fg <path> - Change current directory\n")
		_T(" fj <ld#> - Change current drive\n")
		_T(" fq - Show current directory path\n")
		_T(" fb <name> - Set volume label\n")
		_T(" fm <ld#> <rule> <cluster size> - Create file system\n")
		_T(" fp <pd#> <p1 size> <p2 size> <p3 size> <p4 size> - Divide physical drive\n")
		_T("\n")
	};



int set_console_size (
	HANDLE hcon,
	int width,
	int height,
	int bline
)
{
	COORD dim = {width, bline};
	SMALL_RECT rect = {0, 0, width - 1, height - 1};


	if (SetConsoleScreenBufferSize(hCon, dim) && 
		SetConsoleWindowInfo(hCon, TRUE, &rect) ) return 1;

	return 0;
}




/*-----------------------------------------------------------------------*/
/* Main                                                                  */


int _tmain (int argc, TCHAR *argv[])
{
	TCHAR *ptr, *ptr2, pool[50];
	long p1, p2, p3;
	BYTE *buf;
	UINT s1, s2, cnt;
	WORD w;
	DWORD dw, ofs = 0, sect = 0, drv = 0;
	static const BYTE ft[] = {0, 12, 16, 32};
	FRESULT res;
	FATFS *fs;				/* Pointer to file system object */
	DIR dir;				/* Directory object */
	FIL file[2];			/* File objects */


	hKey = GetStdHandle(STD_INPUT_HANDLE);
	hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	set_console_size(hCon, 100, 35, 500);

	if (GetConsoleCP() != _CODE_PAGE) {
		if (!SetConsoleCP(_CODE_PAGE) || !SetConsoleOutputCP(_CODE_PAGE))
			_tprintf(_T("Error: Failed to change the console code page.\n"));
	}
	_stprintf(pool, _T(".%u"), _CODE_PAGE);
	_tsetlocale(LC_CTYPE, pool);

	_tprintf(_T("FatFs module test monitor (%s, CP:%u/%s)\n\n"),
			_USE_LFN ? _T("LFN") : _T("SFN"),
			_CODE_PAGE,
			_LFN_UNICODE ? _T("Unicode") : _T("ANSI"));

	_stprintf(pool, _T("FatFs debug console (%s, CP:%u/%s)"),
			_USE_LFN ? _T("LFN") : _T("SFN"),
			_CODE_PAGE,
			_LFN_UNICODE ? _T("Unicode") : _T("ANSI"));
	SetConsoleTitle(pool);

	assign_drives();	/* Find physical drives on the PC */

#if _MULTI_PARTITION
	_tprintf(_T("\nMultiple partition feature is enabled. Each logical drive is tied to the patition as follows:\n"));
	for (cnt = 0; cnt < sizeof VolToPart / sizeof (PARTITION); cnt++) {
		const TCHAR *pn[] = {_T("auto detect"), _T("1st partition"), _T("2nd partition"), _T("3rd partition"), _T("4th partition")};

		_tprintf(_T("\"%u:\" <== Disk# %u, %s\n"), cnt, VolToPart[cnt].pd, pn[VolToPart[cnt].pt]);
	}
	_tprintf(_T("\n"));	
#else
	_tprintf(_T("\nMultiple partition feature is disabled.\nEach logical drive is tied to the same physical drive number.\n\n"));
#endif

#if _USE_LFN
	Finfo.lfname = Lfname;
	Finfo.lfsize = sizeof Lfname;
#endif

	for (;;) {
		_tprintf(_T(">"));
		_getts(ptr = Line);	/* Get a line from console */

		switch (*ptr++) {	/* Branch by primary command character */

		case 'q' :	/* Exit program */
			return 0;

		case '?':		/* Show usage */
			_tprintf(HelpStr);
			break;

		case 'T' :
			while (*ptr == ' ') ptr++;

			/* Quick test space */

			break;

		case 'd' :	/* Disk I/O command */
			switch (*ptr++) {	/* Branch by secondary command character */
			case 'd' :	/* dd [<pd#> <sect>] - Dump a secrtor */
				if (!xatoi(&ptr, &p1)) {
					p1 = drv; p2 = sect;
				} else {
					if (!xatoi(&ptr, &p2)) break;
				}
				res = disk_read((BYTE)p1, Buff, p2, 1);
				if (res) { _tprintf(_T("rc=%d\n"), (WORD)res); break; }
				_tprintf(_T("Drive:%u Sector:%lu\n"), p1, p2);
				if (disk_ioctl((BYTE)p1, GET_SECTOR_SIZE, &w) != RES_OK) break;
				sect = p2 + 1; drv = p1;
				for (buf = Buff, ofs = 0; ofs < w; buf += 16, ofs += 16)
					put_dump(buf, ofs, 16);
				break;

			case 'i' :	/* di <pd#> - Initialize physical drive */
				if (!xatoi(&ptr, &p1)) break;
				res = disk_initialize((BYTE)p1);
				_tprintf(_T("rc=%d\n"), res);
				if (disk_ioctl((BYTE)p1, GET_SECTOR_SIZE, &w) == RES_OK)
					_tprintf(_T("Sector size = %u\n"), w);
				if (disk_ioctl((BYTE)p1, GET_SECTOR_COUNT, &dw) == RES_OK)
					_tprintf(_T("Number of sectors = %u\n"), dw);
				break;
			}

			case 'x' :	/* dx <src pd#> <src start sector> <src sector count> <dst pd#> <dst start sector> - Initialize physical drive */
				{
					long sdrv, ssect, scount, ddrv, dsect, xc, xx;

					if (!xatoi(&ptr, &sdrv) || !xatoi(&ptr, &ssect) || !xatoi(&ptr, &scount) || !xatoi(&ptr, &ddrv) || !xatoi(&ptr, &dsect)) break;

					for (xc = 0; xc < scount; xc += xx, ssect += xx, dsect += xx) {
						if ((xc % 2048) == 0 && scount >= 128)
							_tprintf(_T("\r%lu/%lu, %lu%% "), xc, scount, xc / (scount / 100) );
						xx = (scount - xc >= 128) ? 128 : scount - xc;
						res = disk_read((BYTE)sdrv, Buff, ssect, (BYTE)xx);
						if (res) break;
						res = disk_write((BYTE)ddrv, Buff, dsect, (BYTE)xx);
						if (res) break;
					}
					put_rc(res);
				}
			break;

		case 'b' :	/* Buffer control command */
			switch (*ptr++) {	/* Branch by secondary command character */
			case 'd' :	/* bd <ofs> - Dump Buff[] */
				if (!xatoi(&ptr, &p1)) break;
				for (buf = &Buff[p1], ofs = p1, cnt = 32; cnt; cnt--, buf += 16, ofs += 16)
					put_dump(buf, ofs, 16);
				break;

			case 'e' :	/* be <ofs> [<data>] ... - Edit Buff[] */
				if (!xatoi(&ptr, &p1)) break;
				if (xatoi(&ptr, &p2)) {
					do {
						Buff[p1++] = (BYTE)p2;
					} while (xatoi(&ptr, &p2));
					break;
				}
				for (;;) {
					_tprintf(_T("%04X %02X-"), (WORD)(p1), (WORD)Buff[p1]);
					_getts(ptr = Line);
					if (*ptr == '.') break;
					if (*ptr < ' ') { p1++; continue; }
					if (xatoi(&ptr, &p2))
						Buff[p1++] = (BYTE)p2;
					else
						_tprintf(_T("???\n"));
				}
				break;

			case 'r' :	/* br <pd#> <sector> <count> - Read disk into Buff[] */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				_tprintf(_T("rc=%u\n"), disk_read((BYTE)p1, Buff, p2, (BYTE)p3));
				break;

			case 'w' :	/* bw <sect> <count> - Write Buff[] into disk */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				_tprintf(_T("rc=%u\n"), disk_write((BYTE)p1, Buff, p2, (BYTE)p3));
				break;

			case 'f' :	/* bf <n> - Fill Buff[] */
				if (!xatoi(&ptr, &p1)) break;
				memset(Buff, (BYTE)p1, sizeof Buff);
				break;

			}
			break;

		case 'f' :	/* FatFs test command */
			switch (*ptr++) {	/* Branch by secondary command character */

			case 'i' :	/* fi <ld#> - Force initialized the logical drive */
				if (!xatoi(&ptr, &p1)) break;
				put_rc(f_mount((BYTE)p1, &FatFs[p1]));
				break;

			case 's' :	/* fs [<path>] - Show logical drive status */
				while (*ptr == ' ') ptr++;
				ptr2 = ptr;
#if _FS_READONLY
				res = f_opendir(&dir, ptr);
#else
				res = f_getfree(ptr, (DWORD*)&p1, &fs);
#endif
				if (res) { put_rc(res); break; }
				_tprintf(_T("FAT type = FAT%u\nNumber of FATs = %u\n"), ft[fs->fs_type & 3], fs->n_fats);
				_tprintf(_T("Cluster size = %u sectors, %lu bytes\n"),
#if _MAX_SS != 512
					fs->csize, (DWORD)fs->csize * fs->ssize);
#else
					fs->csize, (DWORD)fs->csize * 512);
#endif
				if (fs->fs_type != FS_FAT32) _tprintf(_T("Root DIR entries = %u\n"), fs->n_rootdir);
				_tprintf(_T("Sectors/FAT = %lu\nNumber of clusters = %lu\nVolume start sector = %lu\nFAT start sector = %lu\nRoot DIR start %s = %lu\nData start sector = %lu\n\n"),
					fs->fsize, fs->n_fatent - 2, fs->volbase, fs->fatbase, fs->fs_type == FS_FAT32 ? _T("cluster") : _T("sector"), fs->dirbase, fs->database);
#if _USE_LABEL
				res = f_getlabel(ptr2, pool, &dw);
				if (res) { put_rc(res); break; }
				_tprintf(pool[0] ? _T("Volume name is %s\n") : _T("No volume label\n"), pool);
				_tprintf(_T("Volume S/N is %04X-%04X\n"), dw >> 16, dw & 0xFFFF);
#endif
				_tprintf(_T("..."));
				AccSize = AccFiles = AccDirs = 0;
				res = scan_files(ptr);
				if (res) { put_rc(res); break; }
				p2 = (fs->n_fatent - 2) * fs->csize;
				p3 = p1 * fs->csize;
#if _MAX_SS != 512
				p2 *= fs->ssize / 512;
				p3 *= fs->ssize / 512;
#endif
				p2 /= 2;
				p3 /= 2;
				_tprintf(_T("\r%u files, %I64u bytes.\n%u folders.\n%lu KB total disk space.\n"),
						AccFiles, AccSize, AccDirs, p2);
#if !FS_READONLY
				_tprintf(_T("%lu KB available.\n"), p3);
#endif
				break;

			case 'l' :	/* fl [<path>] - Directory listing */
				while (*ptr == ' ') ptr++;
				res = f_opendir(&dir, ptr);
				if (res) { put_rc(res); break; }
				AccSize = s1 = s2 = 0;
				for(;;) {
					res = f_readdir(&dir, &Finfo);
					if ((res != FR_OK) || !Finfo.fname[0]) break;
					if (Finfo.fattrib & AM_DIR) {
						s2++;
					} else {
						s1++; AccSize += Finfo.fsize;
					}
					_tprintf(_T("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %s"),
							(Finfo.fattrib & AM_DIR) ? 'D' : '-',
							(Finfo.fattrib & AM_RDO) ? 'R' : '-',
							(Finfo.fattrib & AM_HID) ? 'H' : '-',
							(Finfo.fattrib & AM_SYS) ? 'S' : '-',
							(Finfo.fattrib & AM_ARC) ? 'A' : '-',
							(Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,
							(Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63, Finfo.fsize, Finfo.fname);
#if _USE_LFN
					for (p2 = _tcslen(Finfo.fname); p2 < 12; p2++) _tprintf(_T(" "));
					_tprintf(_T("  %s"), Lfname);
#endif
					_tprintf(_T("\n"));
				}
				_tprintf(_T("%4u File(s),%11I64u bytes total\n%4u Dir(s)"), s1, AccSize, s2);
				if (f_getfree(ptr, (DWORD*)&p1, &fs) == FR_OK)
					_tprintf(_T(",%12I64u bytes free\n"), (LONGLONG)p1 * fs->csize * 512);
				break;

			case 'o' :	/* fo <mode> <file> - Open a file */
				if (!xatoi(&ptr, &p1)) break;
				while (*ptr == ' ') ptr++;
				put_rc(f_open(&file[0], ptr, (BYTE)p1));
				break;

			case 'c' :	/* fc - Close a file */
				put_rc(f_close(&file[0]));
				break;

			case 'r' :	/* fr <len> - read file */
				if (!xatoi(&ptr, &p1)) break;
				p2 =0;
				while (p1) {
					if ((UINT)p1 >= sizeof Buff) {
						cnt = sizeof Buff; p1 -= sizeof Buff;
					} else {
						cnt = p1; p1 = 0;
					}
					res = f_read(&file[0], Buff, cnt, &s2);
					if (res != FR_OK) { put_rc(res); break; }
					p2 += s2;
					if (cnt != s2) break;
				}
				_tprintf(_T("%lu bytes read.\n"), p2);
				break;

			case 'd' :	/* fd <len> - read and dump file from current fp */
				if (!xatoi(&ptr, &p1)) p1 = 128;
				ofs = file[0].fptr;
				while (p1) {
					if ((UINT)p1 >= 16) { cnt = 16; p1 -= 16; }
					else 				{ cnt = p1; p1 = 0; }
					res = f_read(&file[0], Buff, cnt, &cnt);
					if (res != FR_OK) { put_rc(res); break; }
					if (!cnt) break;
					put_dump(Buff, ofs, cnt);
					ofs += 16;
				}
				break;

			case 'e' :	/* fe <ofs> - Seek file pointer */
				if (!xatoi(&ptr, &p1)) break;
				res = f_lseek(&file[0], p1);
				put_rc(res);
				if (res == FR_OK)
					_tprintf(_T("fptr = %lu(0x%lX)\n"), file[0].fptr, file[0].fptr);
				break;
#if _USE_FASTSEEK
			case 'E' :	/* fE - Enable fast seek and initialize cluster link map table */
				file[0].cltbl = SeekTbl;			/* Enable fast seek (set address of buffer) */
				SeekTbl[0] = sizeof SeekTbl / sizeof SeekTbl[0];	/* Buffer size */
				res = f_lseek(&file[0], CREATE_LINKMAP);	/* Create link map table */
				put_rc(res);
				if (res == FR_OK) {
					_tprintf(_T("%u clusters, "), (file[0].fsize + 1) );
					_tprintf((SeekTbl[0] > 4) ? _T("fragmented in %d.\n") : _T("contiguous.\n"), SeekTbl[0] / 2 - 1);
					_tprintf(_T("%u items used.\n"), SeekTbl[0]);

				}
				if (res == FR_NOT_ENOUGH_CORE) {
					_tprintf(_T("%u items required to create the link map table.\n"), SeekTbl[0]);
				}
				break;
#endif	/* _USE_FASTSEEK */
#if _FS_RPATH >= 1
			case 'g' :	/* fg <path> - Change current directory */
				while (*ptr == ' ') ptr++;
				put_rc(f_chdir(ptr));
				break;

			case 'j' :	/* fj <ld#> - Change current drive */
				if (xatoi(&ptr, &p1)) {
					put_rc(f_chdrive((BYTE)p1));
				}
				break;
#if _FS_RPATH >= 2
			case 'q' :	/* fq - Show current dir path */
				res = f_getcwd(Line, 256);
				if (res) {
					put_rc(res);
				} else {
					WriteConsole(hCon, Line, _tcslen(Line), &p1, NULL);
					_tprintf(_T("\n"));
				}
				break;
#endif	/* _FS_RPATH >= 2 */
#endif	/* _FS_RPATH >= 1 */
#if !_FS_READONLY
			case 'w' :	/* fw <len> <val> - write file */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				memset(Buff, (BYTE)p2, sizeof Buff);
				p2 = 0;
				while (p1) {
					if ((UINT)p1 >= sizeof Buff) { cnt = sizeof Buff; p1 -= sizeof Buff; }
					else 				  { cnt = p1; p1 = 0; }
					res = f_write(&file[0], Buff, cnt, &s2);
					if (res != FR_OK) { put_rc(res); break; }
					p2 += s2;
					if (cnt != s2) break;
				}
				_tprintf(_T("%lu bytes written.\n"), p2);
				break;

			case 'v' :	/* fv - Truncate file */
				put_rc(f_truncate(&file[0]));
				break;

			case 'n' :	/* fn <name> <new_name> - Change file/dir name */
				while (*ptr == ' ') ptr++;
				ptr2 = _tcschr(ptr, ' ');
				if (!ptr2) break;
				*ptr2++ = 0;
				while (*ptr2 == ' ') ptr2++;
				put_rc(f_rename(ptr, ptr2));
				break;

			case 'u' :	/* fu <name> - Unlink a file/dir */
				while (*ptr == ' ') ptr++;
				put_rc(f_unlink(ptr));
				break;

			case 'k' :	/* fk <name> - Create a directory */
				while (*ptr == ' ') ptr++;
				put_rc(f_mkdir(ptr));
				break;

			case 'a' :	/* fa <atrr> <mask> <name> - Change file/dir attribute */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				while (*ptr == ' ') ptr++;
				put_rc(f_chmod(ptr, (BYTE)p1, (BYTE)p2));
				break;

			case 't' :	/* ft <year> <month> <day> <hour> <min> <sec> <name> - Change timestamp of a file/dir */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				Finfo.fdate = (WORD)(((p1 - 1980) << 9) | ((p2 & 15) << 5) | (p3 & 31));
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				Finfo.ftime = (WORD)(((p1 & 31) << 11) | ((p2 & 63) << 5) | ((p3 >> 1) & 31));
				while (_USE_LFN && *ptr == ' ') ptr++;
				put_rc(f_utime(ptr, &Finfo));
				break;

			case 'x' : /* fx <src_name> <dst_name> - Copy a file */
				while (*ptr == ' ') ptr++;
				ptr2 = _tcschr(ptr, ' ');
				if (!ptr2) break;
				*ptr2++ = 0;
				while (*ptr2 == ' ') ptr2++;
				_tprintf(_T("Opening \"%s\""), ptr);
				res = f_open(&file[0], ptr, FA_OPEN_EXISTING | FA_READ);
				_tprintf(_T("\n"));
				if (res) {
					put_rc(res);
					break;
				}
				while (*ptr2 == ' ') ptr2++;
				_tprintf(_T("Creating \"%s\""), ptr2);
				res = f_open(&file[1], ptr2, FA_CREATE_ALWAYS | FA_WRITE);
				_tprintf(_T("\n"));
				if (res) {
					put_rc(res);
					f_close(&file[0]);
					break;
				}
				_tprintf(_T("Copying..."));
				p1 = 0;
				for (;;) {
					res = f_read(&file[0], Buff, sizeof Buff, &s1);
					if (res || s1 == 0) break;   /* error or eof */
					res = f_write(&file[1], Buff, s1, &s2);
					p1 += s2;
					if (res || s2 < s1) break;   /* error or disk full */
				}
				_tprintf(_T("\n"));
				if (res) put_rc(res);
				f_close(&file[0]);
				f_close(&file[1]);
				_tprintf(_T("%lu bytes copied.\n"), p1);
				break;
#if _USE_LABEL
			case 'b' :	/* fb <name> - Set volume label */
				while (*ptr == ' ') ptr++;
				put_rc(f_setlabel(ptr));
				break;
#endif	/* USE_LABEL */
#if _USE_MKFS
			case 'm' :	/* fm <ld#> <partition rule> <cluster size> - Create file system */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				_tprintf(_T("The volume will be formatted. Are you sure? (Y/n)="));
				_fgetts(ptr, 256, stdin);
				if (*ptr != 'Y') break;
				put_rc(f_mkfs((BYTE)p1, (BYTE)p2, (UINT)p3));
				break;
#if _MULTI_PARTITION
			case 'p' :	/* fp <pd#> <size1> <size2> <size3> <size4> - Create partition table */
				{
					DWORD pts[4];

					if (!xatoi(&ptr, &p1)) break;
					xatoi(&ptr, &pts[0]);
					xatoi(&ptr, &pts[1]);
					xatoi(&ptr, &pts[2]);
					xatoi(&ptr, &pts[3]);
					_tprintf(_T("The physical drive %u will be re-partitioned. Are you sure? (Y/n)="), p1);
					_fgetts(ptr, 256, stdin);
					if (*ptr != 'Y') break;
					put_rc(f_fdisk((BYTE)p1, pts, Buff));
				}
				break;
#endif	/* _MULTI_PARTITION */
#endif	/* _USE_MKFS */
#endif	/* !_FS_READONLY */
			}
			break;

		}
	}

}



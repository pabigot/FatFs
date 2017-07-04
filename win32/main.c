/*------------------------------------------------------------------------/
/  The Main Development Bench of FatFs Module
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2017, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/


#include <string.h>
#include <stdio.h>
#include <locale.h>
#include "diskio.h"
#include "ff.h"

int assign_drives (void);	/* Initialization of low level I/O module */


#if FF_MULTI_PARTITION

This is an example of volume - partition resolution table.

PARTITION VolToPart[] = {
	{0, 0},	/* "0:" <== PD# 0, auto detect */
	{1, 0},	/* "1:" <== PD# 1, auto detect */
	{2, 0},	/* "2:" <== PD# 2, auto detect */
	{3, 1},	/* "3:" <== PD# 3, 1st partition */
	{3, 2},	/* "4:" <== PD# 3, 2nd partition */
	{3, 3},	/* "5:" <== PD# 3, 3rd partition */
	{4, 0},	/* "6:" <== PD# 4, auto detect */
	{5, 0}	/* "7:" <== PD# 5, auto detect */
};
#endif


#ifdef UNICODE
#if !FF_LFN_UNICODE
#error FF_LFN_UNICODE must be 1 in this build configuration.
#endif
#else
#if FF_LFN_UNICODE
#error FF_LFN_UNICODE must be 0 in this build configuration.
#endif
#endif





/*---------------------------------------------------------*/
/* Work Area                                               */
/*---------------------------------------------------------*/

LONGLONG AccSize;			/* Work register for scan_files() */
WORD AccFiles, AccDirs;
FILINFO Finfo;

FILE *AutoExec;
TCHAR Line[300];			/* Console input/output buffer */
HANDLE hCon, hKey;

FATFS FatFs[FF_VOLUMES];		/* Filesystem object for logical drive */
BYTE Buff[262144];			/* Working buffer */

#if FF_USE_FASTSEEK
DWORD SeekTbl[16];			/* Link map table for fast seek feature */
#endif


/*-------------------------------------------------------------------*/
/* User Provided RTC Function for FatFs module                       */
/*-------------------------------------------------------------------*/
/* This is a real time clock service to be called from FatFs module. */
/* This function is needed when FF_FS_READONLY == 0 and FF_FS_NORTC == 0 */

DWORD get_fattime (void)
{
	SYSTEMTIME tm;

	/* Get local time */
	GetLocalTime(&tm);

	/* Pack date and time into a DWORD variable */
	return    ((DWORD)(tm.wYear - 1980) << 25)
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

int xatoll (		/* 0:Failed, 1:Successful */
	TCHAR **str,	/* Pointer to pointer to the string */
	QWORD *res		/* Pointer to a valiable to store the value */
)
{
	QWORD val;
	unsigned char r;
	TCHAR c;


	*res = 0;
	while ((c = **str) == ' ') (*str)++;	/* Skip leading spaces */

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

	*res = val;
	return 1;
}


int xatoi (
	TCHAR **str,	/* Pointer to pointer to the string */
	DWORD *res		/* Pointer to a valiable to store the value */
)
{
	QWORD d;


	*res = 0;
	if (!xatoll(str, &d)) return 0;
	*res = (DWORD)d;
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

	for (i = 0; i < cnt; i++) {
		_tprintf(_T(" %02X"), buff[i]);
	}

	_puttchar(' ');
	for (i = 0; i < cnt; i++) {
		_puttchar((TCHAR)((buff[i] >= ' ' && buff[i] <= '~') ? buff[i] : '.'));
	}

	_puttchar('\n');
}



UINT forward (
	const BYTE* buf,
	UINT btf
)
{
	UINT i;


	if (btf) {	/* Transfer call? */
		for (i = 0; i < btf; i++) _puttchar(buf[i]);
		return btf;
	} else {	/* Sens call */
		return 1;
	}
}



FRESULT scan_files (
	TCHAR* path		/* Pointer to the path name working buffer */
)
{
	DIR dir;
	FRESULT res;
	int i;


	if ((res = f_opendir(&dir, path)) == FR_OK) {
		i = _tcslen(path);
		while (((res = f_readdir(&dir, &Finfo)) == FR_OK) && Finfo.fname[0]) {
			if (Finfo.fattrib & AM_DIR) {
				AccDirs++;
				*(path+i) = '/'; _tcscpy(path+i+1, Finfo.fname);
				res = scan_files(path);
				*(path+i) = '\0';
				if (res != FR_OK) break;
			} else {
//				_tprintf(_T("%s/%s\n"), path, Finfo.fname);
				AccFiles++;
				AccSize += Finfo.fsize;
			}
		}
		f_closedir(&dir);
	}

	return res;
}



void put_rc (FRESULT rc)
{
	const TCHAR *p =
		_T("OK\0DISK_ERR\0INT_ERR\0NOT_READY\0NO_FILE\0NO_PATH\0INVALID_NAME\0")
		_T("DENIED\0EXIST\0INVALID_OBJECT\0WRITE_PROTECTED\0INVALID_DRIVE\0")
		_T("NOT_ENABLED\0NO_FILE_SYSTEM\0MKFS_ABORTED\0TIMEOUT\0LOCKED\0")
		_T("NOT_ENOUGH_CORE\0TOO_MANY_OPEN_FILES\0INVALID_PARAMETER\0");
	FRESULT i;

	for (i = 0; i != rc && *p; i++) {
		while(*p++) ;
	}
	_tprintf(_T("rc=%u FR_%s\n"), (UINT)rc, p);
}



const TCHAR HelpStr[] = {
		_T("[Disk contorls]\n")
		_T(" di <pd#> - Initialize disk\n")
		_T(" dd [<pd#> <sect>] - Dump a secrtor\n")
		_T(" ds <pd#> - Show disk status\n")
		_T(" dl <file> - Load FAT image into RAM disk (pd#0)\n")
		_T("[Buffer contorls]\n")
		_T(" bd <ofs> - Dump working buffer\n")
		_T(" be <ofs> [<data>] ... - Edit working buffer\n")
		_T(" br <pd#> <sect> <count> - Read disk into working buffer\n")
		_T(" bw <pd#> <sect> <count> - Write working buffer into disk\n")
		_T(" bf <val> - Fill working buffer\n")
		_T("[Filesystem contorls]\n")
		_T(" fi <ld#> [<opt>] - Force initialized the volume\n")
		_T(" fs [<path>] - Show volume status\n")
		_T(" fl [<path>] - Show a directory\n")
		_T(" fL <path> <pat> - Find a directory\n")
		_T(" fo <mode> <file> - Open a file\n")
		_T(" fc - Close the file\n")
		_T(" fe <ofs> - Move fp in normal seek\n")
		_T(" fE <ofs> - Move fp in fast seek or Create link table\n")
		_T(" ff <len> - Forward file data to the console\n")
		_T(" fh <fsz> <opt> - Allocate a contiguous block to the file\n")
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
		_T(" fj <path> - Change current drive\n")
		_T(" fq - Show current directory path\n")
		_T(" fb <name> - Set volume label\n")
		_T(" fm <ld#> <type> <au> - Create FAT volume\n")
		_T(" fp <pd#> <p1 size> <p2 size> <p3 size> <p4 size> - Divide physical drive\n")
		_T(" p <cp#> - Set code page\n")
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



void get_uni (
	TCHAR* buf,
	UINT len
)
{
	UINT i = 0;
	DWORD n;


	if (AutoExec) {
		if (_fgetts(buf, len, AutoExec)) {
			for (i = 0; buf[i] >= 0x20 && i < len - 1; i++) ;
			if (i) {
				buf[i] = 0;
				_putts(buf);
				return;
			}
		}
		fclose(AutoExec);
		AutoExec = 0;
	}
	for (;;) {
		ReadConsole(hKey, &buf[i], 1, &n, 0);
		if (buf[i] == 8) {
			if (i) i--;
			continue;
		}
		if (buf[i] == 13) {
			buf[i] = 0;
			break;
		}
		if ((UINT)buf[i] >= ' ' && i + n < len) i += n;
	}
}


void put_uni (
	TCHAR* buf
)
{
	DWORD n;


	while (*buf) {
		WriteConsole(hCon, buf, 1, &n, 0);
		buf++;
	}
}




/*-----------------------------------------------------------------------*/
/* Main                                                                  */
/*-----------------------------------------------------------------------*/


int _tmain (int argc, TCHAR *argv[])
{
	TCHAR *ptr, *ptr2, pool[50];
	DWORD p1, p2, p3;
	QWORD px;
	BYTE *buf;
	UINT s1, s2, cnt;
	WORD w;
	DWORD dw, ofs = 0, sect = 0, drv = 0;
	const TCHAR *ft[] = {_T(""), _T("FAT12"), _T("FAT16"), _T("FAT32"), _T("exFAT")};
	HANDLE h;
	FRESULT fr;
	DRESULT dr;
	FATFS *fs;				/* Pointer to file system object */
	DIR dir;				/* Directory object */
	FIL file[2];			/* File objects */


	hKey = GetStdHandle(STD_INPUT_HANDLE);
	hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	set_console_size(hCon, 100, 35, 500);

#if FF_CODE_PAGE != 0
	if (GetConsoleCP() != FF_CODE_PAGE) {
		if (!SetConsoleCP(FF_CODE_PAGE) || !SetConsoleOutputCP(FF_CODE_PAGE)) {
			_tprintf(_T("Error: Failed to change the console code page.\n"));
		}
	}
#else
	w = GetConsoleCP();
	_tprintf(_T("f_setcp(%u)\n"), w);
	put_rc(f_setcp(w));
#endif

	_tprintf(_T("FatFs module test monitor (%s, CP:%u/%s)\n\n"),
			FF_USE_LFN ? _T("LFN") : _T("SFN"),
			FF_CODE_PAGE,
			FF_LFN_UNICODE ? _T("Unicode") : _T("ANSI"));

	_stprintf(pool, _T("FatFs debug console (%s, CP:%u/%s)"),
			FF_USE_LFN ? _T("LFN") : _T("SFN"),
			FF_CODE_PAGE,
			FF_LFN_UNICODE ? _T("Unicode") : _T("ANSI"));
	SetConsoleTitle(pool);

	assign_drives();	/* Find physical drives on the PC */

#if FF_MULTI_PARTITION
	_tprintf(_T("\nMultiple partition is enabled. Each logical drive is tied to the patition as follows:\n"));
	for (cnt = 0; cnt < sizeof VolToPart / sizeof (PARTITION); cnt++) {
		const TCHAR *pn[] = {_T("auto detect"), _T("1st partition"), _T("2nd partition"), _T("3rd partition"), _T("4th partition")};

		_tprintf(_T("\"%u:\" <== Disk# %u, %s\n"), cnt, VolToPart[cnt].pd, pn[VolToPart[cnt].pt]);
	}
	_tprintf(_T("\n"));
#else
	_tprintf(_T("\nMultiple partition is disabled.\nEach logical drive is tied to the same physical drive number.\n\n"));
#endif

	AutoExec = _tfopen(_T("run.txt"), _T("rt"));


	for (;;) {
		_tprintf(_T(">"));
		get_uni(Line, sizeof Line / sizeof *Line);
		ptr = Line;

		switch (*ptr++) {	/* Branch by primary command character */

		case 'q' :	/* Exit program */
			return 0;

		case '?':		/* Show usage */
			_tprintf(HelpStr);
			break;

		case 'T' :

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
				dr = disk_read((BYTE)p1, Buff, p2, 1);
				if (dr) { _tprintf(_T("rc=%d\n"), (WORD)dr); break; }
				_tprintf(_T("Drive:%u Sector:%lu\n"), p1, p2);
				if (disk_ioctl((BYTE)p1, GET_SECTOR_SIZE, &w) != RES_OK) break;
				sect = p2 + 1; drv = p1;
				for (buf = Buff, ofs = 0; ofs < w; buf += 16, ofs += 16) {
					put_dump(buf, ofs, 16);
				}
				break;

			case 'i' :	/* di <pd#> - Initialize physical drive */
				if (!xatoi(&ptr, &p1)) break;
				dr = disk_initialize((BYTE)p1);
				_tprintf(_T("rc=%d\n"), dr);
				if (disk_ioctl((BYTE)p1, GET_SECTOR_SIZE, &w) == RES_OK) {
					_tprintf(_T("Sector size = %u\n"), w);
				}
				if (disk_ioctl((BYTE)p1, GET_SECTOR_COUNT, &dw) == RES_OK) {
					_tprintf(_T("Number of sectors = %u\n"), dw);
				}
				break;

			case 'l' :	/* dl <image file> - Load image of a FAT volume into RAM disk */
				while (*ptr == ' ') ptr++;
				if (disk_ioctl(0, 200, ptr) == RES_OK) {
					_tprintf(_T("Ok\n"));
				}
				break;

			}
			break;

		case 'b' :	/* Buffer control command */
			switch (*ptr++) {	/* Branch by secondary command character */
			case 'd' :	/* bd <ofs> - Dump Buff[] */
				if (!xatoi(&ptr, &p1)) break;
				for (buf = &Buff[p1], ofs = p1, cnt = 32; cnt; cnt--, buf += 16, ofs += 16) {
					put_dump(buf, ofs, 16);
				}
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
					get_uni(Line, sizeof Line / sizeof *Line);
					ptr = Line;
					if (*ptr == '.') break;
					if (*ptr < ' ') { p1++; continue; }
					if (xatoi(&ptr, &p2)) {
						Buff[p1++] = (BYTE)p2;
					} else {
						_tprintf(_T("???\n"));
					}
				}
				break;

			case 'r' :	/* br <pd#> <sector> <count> - Read disk into Buff[] */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				_tprintf(_T("rc=%u\n"), disk_read((BYTE)p1, Buff, p2, p3));
				break;

			case 'w' :	/* bw <pd#> <sect> <count> - Write Buff[] into disk */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				_tprintf(_T("rc=%u\n"), disk_write((BYTE)p1, Buff, p2, p3));
				break;

			case 'f' :	/* bf <n> - Fill Buff[] */
				if (!xatoi(&ptr, &p1)) break;
				memset(Buff, p1, sizeof Buff);
				break;

			case 's' :	/* bs - Save Buff[] */
				while (*ptr == ' ') ptr++;
				h = CreateFile(_T("Buff.bin"), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
				if (h != INVALID_HANDLE_VALUE) {
					WriteFile(h, Buff, sizeof Buff, &dw, 0);
					CloseHandle(h);
					_tprintf(_T("Ok.\n"));
				}
				break;

			}
			break;

		case 'f' :	/* FatFs test command */
			switch (*ptr++) {	/* Branch by secondary command character */

			case 'i' :	/* fi <ld#> [<mount>] - Force initialized the logical drive */
				if (!xatoi(&ptr, &p1) || (UINT)p1 > 9) break;
				if (!xatoi(&ptr, &p2)) p2 = 0;
				_stprintf(ptr, _T("%d:"), p1);
				put_rc(f_mount(&FatFs[p1], ptr, (BYTE)p2));
				break;

			case 's' :	/* fs [<path>] - Show logical drive status */
				while (*ptr == ' ') ptr++;
				ptr2 = ptr;
#if FF_FS_READONLY
				fr = f_opendir(&dir, ptr);
				if (fr == FR_OK) {
					fs = dir.obj.fs;
					f_closedir(&dir);
				}
#else
				fr = f_getfree(ptr, (DWORD*)&p1, &fs);
#endif
				if (fr) { put_rc(fr); break; }
				_tprintf(_T("FAT type = %s\n"), ft[fs->fs_type]);
				_tprintf(_T("Cluster size = %lu bytes\n"),
#if FF_MAX_SS != FF_MIN_SS
					(DWORD)fs->csize * fs->ssize);
#else
					(DWORD)fs->csize * FF_MAX_SS);
#endif
				if (fs->fs_type < FS_FAT32) _tprintf(_T("Root DIR entries = %u\n"), fs->n_rootdir);
				_tprintf(_T("Sectors/FAT = %lu\nNumber of FATs = %u\nNumber of clusters = %lu\nVolume start sector = %lu\nFAT start sector = %lu\nRoot DIR start %s = %lu\nData start sector = %lu\n\n"),
					fs->fsize, fs->n_fats, fs->n_fatent - 2, fs->volbase, fs->fatbase, fs->fs_type >= FS_FAT32 ? _T("cluster") : _T("sector"), fs->dirbase, fs->database);
#if FF_USE_LABEL
				fr = f_getlabel(ptr2, pool, &dw);
				if (fr) { put_rc(fr); break; }
				if (pool[0]) {
					_tprintf(_T("Volume name is "));
					put_uni(pool);
					_tprintf(_T("\n"));
				} else {
					_tprintf(_T("No volume label\n"));
				}
				_tprintf(_T("Volume S/N is %04X-%04X\n"), dw >> 16, dw & 0xFFFF);
#endif
				_tprintf(_T("..."));
				AccSize = AccFiles = AccDirs = 0;
				fr = scan_files(ptr);
				if (fr) { put_rc(fr); break; }
				p2 = (fs->n_fatent - 2) * fs->csize;
				p3 = p1 * fs->csize;
#if FF_MAX_SS != FF_MIN_SS
				p2 *= fs->ssize / 512;
				p3 *= fs->ssize / 512;
#endif
				p2 /= 2;
				p3 /= 2;
				_tprintf(_T("\r%u files, %I64u bytes.\n%u folders.\n%lu KiB total disk space.\n"),
						AccFiles, AccSize, AccDirs, p2);
#if !FF_FS_READONLY
				_tprintf(_T("%lu KiB available.\n"), p3);
#endif
				break;

			case 'l' :	/* fl [<path>] - Directory listing */
				while (*ptr == ' ') ptr++;
				fr = f_opendir(&dir, ptr);
				if (fr) { put_rc(fr); break; }
				AccSize = s1 = s2 = 0;
				for(;;) {
					fr = f_readdir(&dir, &Finfo);
					if ((fr != FR_OK) || !Finfo.fname[0]) break;
					if (Finfo.fattrib & AM_DIR) {
						s2++;
					} else {
						s1++; AccSize += Finfo.fsize;
					}
					_tprintf(_T("%c%c%c%c%c %u/%02u/%02u %02u:%02u %10I64u  "),
							(Finfo.fattrib & AM_DIR) ? 'D' : '-',
							(Finfo.fattrib & AM_RDO) ? 'R' : '-',
							(Finfo.fattrib & AM_HID) ? 'H' : '-',
							(Finfo.fattrib & AM_SYS) ? 'S' : '-',
							(Finfo.fattrib & AM_ARC) ? 'A' : '-',
							(Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,
							(Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63, (QWORD)Finfo.fsize);
#if FF_USE_LFN && FF_USE_FIND == 2
					_tprintf(_T("%-12s  "),Finfo.altname);
#endif
					put_uni(Finfo.fname);
					_tprintf(_T("\n"));
				}
				f_closedir(&dir);
				_tprintf(_T("%4u File(s),%11I64u bytes total\n%4u Dir(s)"), s1, AccSize, s2);
#if !FF_FS_READONLY
				if (f_getfree(ptr, (DWORD*)&p1, &fs) == FR_OK) {
					_tprintf(_T(",%12I64u bytes free"), (QWORD)p1 * fs->csize * 512);
				}
#endif
				_tprintf(_T("\n"));
				break;
#if FF_USE_FIND
			case 'L' :	/* fL <path> <pattern> - Directory search */
				while (*ptr == ' ') ptr++;
				ptr2 = ptr;
				while (*ptr != ' ') ptr++;
				*ptr++ = 0;
				fr = f_findfirst(&dir, &Finfo, ptr2, ptr);
				while (fr == FR_OK && Finfo.fname[0]) {
#if FF_USE_LFN && FF_USE_FIND == 2
					_tprintf(_T("%-12s  "), Finfo.altname);
#endif
					put_uni(Finfo.fname);
					_tprintf(_T("\n"));
					fr = f_findnext(&dir, &Finfo);
				}
				if (fr) put_rc(fr);

				f_closedir(&dir);
				break;
#endif
			case 'o' :	/* fo <mode> <file> - Open a file */
				if (!xatoi(&ptr, &p1)) break;
				while (*ptr == ' ') ptr++;
				fr = f_open(&file[0], ptr, (BYTE)p1);
				put_rc(fr);
				break;

			case 'c' :	/* fc - Close a file */
				put_rc(f_close(&file[0]));
				break;

			case 'r' :	/* fr <len> - read file */
				if (!xatoi(&ptr, &p1)) break;
				p2 =0;
				while (p1) {
					if (p1 >= sizeof Buff) {
						cnt = sizeof Buff; p1 -= sizeof Buff;
					} else {
						cnt = p1; p1 = 0;
					}
					fr = f_read(&file[0], Buff, cnt, &s2);
					if (fr != FR_OK) { put_rc(fr); break; }
					p2 += s2;
					if (cnt != s2) break;
				}
				_tprintf(_T("%lu bytes read.\n"), p2);
				break;

			case 'd' :	/* fd <len> - read and dump file from current fp */
				if (!xatoi(&ptr, &p1)) p1 = 128;
				ofs = (DWORD)file[0].fptr;
				while (p1) {
					if (p1 >= 16) { cnt = 16; p1 -= 16; }
					else 		  { cnt = p1; p1 = 0; }
					fr = f_read(&file[0], Buff, cnt, &cnt);
					if (fr != FR_OK) { put_rc(fr); break; }
					if (!cnt) break;
					put_dump(Buff, ofs, cnt);
					ofs += 16;
				}
				break;

			case 'e' :	/* fe <ofs> - Seek file pointer */
				if (!xatoll(&ptr, &px)) break;
				fr = f_lseek(&file[0], (FSIZE_t)px);
				put_rc(fr);
				if (fr == FR_OK) {
					_tprintf(_T("fptr = %I64u(0x%I64X)\n"), (QWORD)file[0].fptr, (QWORD)file[0].fptr);
				}
				break;
#if FF_USE_FASTSEEK
			case 'E' :	/* fE - Enable fast seek and initialize cluster link map table */
				file[0].cltbl = SeekTbl;			/* Enable fast seek (set address of buffer) */
				SeekTbl[0] = sizeof SeekTbl / sizeof *SeekTbl;	/* Buffer size */
				fr = f_lseek(&file[0], CREATE_LINKMAP);	/* Create link map table */
				put_rc(fr);
				if (fr == FR_OK) {
					_tprintf((SeekTbl[0] > 4) ? _T("fragmented in %d.\n") : _T("contiguous.\n"), SeekTbl[0] / 2 - 1);
					_tprintf(_T("%u items used.\n"), SeekTbl[0]);

				}
				if (fr == FR_NOT_ENOUGH_CORE) {
					_tprintf(_T("%u items required to create the link map table.\n"), SeekTbl[0]);
				}
				break;
#endif	/* FF_USE_FASTSEEK */
#if FF_FS_RPATH >= 1
			case 'g' :	/* fg <path> - Change current directory */
				while (*ptr == ' ') ptr++;
				put_rc(f_chdir(ptr));
				break;
#if FF_VOLUMES >= 2
			case 'j' :	/* fj <path> - Change current drive */
				while (*ptr == ' ') ptr++;
				put_rc(f_chdrive(ptr));
				break;
#endif
#if FF_FS_RPATH >= 2
			case 'q' :	/* fq - Show current dir path */
				fr = f_getcwd(Line, 256);
				if (fr) {
					put_rc(fr);
				} else {
					WriteConsole(hCon, Line, _tcslen(Line), &p1, NULL);
					_tprintf(_T("\n"));
				}
				break;
#endif
#endif
#if FF_USE_FORWARD
			case 'f' :	/* ff <len> - Forward data */
				if (!xatoi(&ptr, &p1)) break;
				fr = f_forward(&file[0], forward, p1, &s1);
				put_rc(fr);
				if (fr == FR_OK) _tprintf(_T("\n%u bytes tranferred.\n"), s1);
				break;
#endif
#if !FF_FS_READONLY
			case 'w' :	/* fw <len> <val> - Write file */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				memset(Buff, p2, sizeof Buff);
				p2 = 0;
				while (p1) {
					if (p1 >= sizeof Buff) { cnt = sizeof Buff; p1 -= sizeof Buff; }
					else 				  { cnt = p1; p1 = 0; }
					fr = f_write(&file[0], Buff, cnt, &s2);
					if (fr != FR_OK) { put_rc(fr); break; }
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

			case 'x' : /* fx <src_name> <dst_name> - Copy a file */
				while (*ptr == ' ') ptr++;
				ptr2 = _tcschr(ptr, ' ');
				if (!ptr2) break;
				*ptr2++ = 0;
				while (*ptr2 == ' ') ptr2++;
				_tprintf(_T("Opening \"%s\""), ptr);
				fr = f_open(&file[0], ptr, FA_OPEN_EXISTING | FA_READ);
				_tprintf(_T("\n"));
				if (fr) {
					put_rc(fr);
					break;
				}
				while (*ptr2 == ' ') ptr2++;
				_tprintf(_T("Creating \"%s\""), ptr2);
				fr = f_open(&file[1], ptr2, FA_CREATE_ALWAYS | FA_WRITE);
				_tprintf(_T("\n"));
				if (fr) {
					put_rc(fr);
					f_close(&file[0]);
					break;
				}
				_tprintf(_T("Copying..."));
				p1 = 0;
				for (;;) {
					fr = f_read(&file[0], Buff, sizeof Buff, &s1);
					if (fr || s1 == 0) break;   /* error or eof */
					fr = f_write(&file[1], Buff, s1, &s2);
					p1 += s2;
					if (fr || s2 < s1) break;   /* error or disk full */
				}
				_tprintf(_T("\n"));
				if (fr) put_rc(fr);
				f_close(&file[0]);
				f_close(&file[1]);
				_tprintf(_T("%lu bytes copied.\n"), p1);
				break;
#if FF_USE_EXPAND
			case 'h':	/* fh <fsz> <opt> - Allocate contiguous block */
				if (!xatoll(&ptr, &px) || !xatoi(&ptr, &p2)) break;
				fr = f_expand(&file[0], (FSIZE_t)px, (BYTE)p2);
				put_rc(fr);
				break;
#endif
#if FF_USE_CHMOD
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
				while (FF_USE_LFN && *ptr == ' ') ptr++;
				put_rc(f_utime(ptr, &Finfo));
				break;
#endif
#if FF_USE_LABEL
			case 'b' :	/* fb <name> - Set volume label */
				while (*ptr == ' ') ptr++;
				put_rc(f_setlabel(ptr));
				break;
#endif
#if FF_USE_MKFS
			case 'm' :	/* fm <ld#> <partition rule> <cluster size> - Create filesystem */
				if (!xatoi(&ptr, &p1) || (UINT)p1 > 9 || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				_tprintf(_T("The volume will be formatted. Are you sure? (Y/n)="));
				get_uni(ptr, 256);
				if (*ptr != _T('Y')) break;
				_stprintf(ptr, _T("%u:"), (UINT)p1);
				put_rc(f_mkfs(ptr, (BYTE)p2, p3, Buff, sizeof Buff));
				break;
#if FF_MULTI_PARTITION
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
					put_rc(f_fdisk(p1, pts, Buff));
				}
				break;
#endif	/* FF_MULTI_PARTITION */
#endif	/* FF_USE_MKFS */
#endif	/* !FF_FS_READONLY */
			}
			break;
#if FF_CODE_PAGE == 0
			case 'p' :	/* p <codepage> - Set code page */
			if (!xatoi(&ptr, &p1)) break;
			fr = f_setcp((WORD)p1);
			put_rc(fr);
			if (fr == FR_OK) {
				if (SetConsoleCP(p1) && SetConsoleOutputCP(p1)) {
					_tprintf(_T("Console CP = %u\n"), p1);
				} else {
					_tprintf(_T("Failed to change the console CP.\n"));
				}
			}
			break;
#endif

		}
	}

}



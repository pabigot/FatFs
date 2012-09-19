/*----------------------------------------------------------------------*/
/* FatFs Module Sample Program / Renesas SH7262        (C)ChaN, 2010    */
/*----------------------------------------------------------------------*/
/* Ev.Board: FRK-SH2A from CQ Publishing                                */
/* Console: SCI2 (N81 38400bps)                                         */
/* MMC/SDC: RSPI0 (SPI mode)                                            */
/*----------------------------------------------------------------------*/


#include <machine.h>
#include "iodefine.h"
#include "vect.h"
#include "integer.h"
#include "scif.h"
#include "xprintf.h"
#include "diskio.h"
#include "ff.h"

#define	PCLK	24000000UL

extern void disk_timerproc (void);


/*---------------------------------------------------------*/
/* Work Area                                               */
/*---------------------------------------------------------*/

DWORD AccSize;				/* Working variables (fs command) */
WORD AccFiles, AccDirs;

FILINFO Finfo;				/* Working variables (fs/fl command) */
#if _USE_LFN
char Lfname[_MAX_LFN + 1];
#endif

char Line[256];				/* Console input buffer */

FATFS Fatfs;				/* File system object */
FIL File1, File2;			/* File objects */
DIR Dir;					/* Directory object */

BYTE Buff[32768];			/* Data buffer */

volatile UINT Timer;		/* Performance timer (1kHz increment) */



/*---------------------------------------------------------*/
/* 1000Hz interval timer (CMT0)                            */
/*---------------------------------------------------------*/

void INT_CMT_CMI0 (void)	/* ISR: Requires vect.h */
{
	static int div10, b;


	CMT.CMCSR0.BIT.CMF = 0;		/* Clear CMT0 interrupt flag */

	Timer++;

	if (++div10 >= 10) {
		div10 = 0;
		disk_timerproc();		/* Disk timer function (100Hz) */
	}
}


static
void delay_ms (					/* Delay in unit of ms */
	UINT ms
)
{
	for (Timer = 0; Timer < ms; ) ;
}



/*---------------------------------------------------------*/
/* Initialize peripherals                                  */
/*---------------------------------------------------------*/


#define SetCCR1(x) ((void(*)(DWORD))((DWORD)SetCCR1_proc + 0x20000000))(x)	/* Write data to CCR1 (Executed at non-cacheable area) */
void SetCCR1_proc (DWORD val)	/* Write a value to CCR1 */
{
	CCNT.CCR1.LONG;
	CCNT.CCR1.LONG;
	CCNT.CCR1.LONG = val;
	CCNT.CCR1.LONG;
	CCNT.CCR1.LONG;
}


static
void IoInit (void)
{
	CPG.FRQCR.WORD = 0x0104;		/* Clock configuration: I=144M, B=48M, P=24M */
	SetCCR1(0x0000090B);			/* Enable instruction/data cache */

	CPG.STBCR3.BYTE = 0;			/* Enable all peripherals */
	CPG.STBCR4.BYTE = 0;
	CPG.STBCR5.BYTE = 0;
	CPG.STBCR6.BYTE = 0;
	CPG.STBCR7.BYTE = 0;
	CPG.STBCR8.BYTE = 0;

	scif2_init();					/* Initialize SCI2: N81,38400bps */

	/* Initialize CMT0 (1kHz interval timer) */
	CMT.CMCNT0.WORD = 0;			/* Clear counter */
	CMT.CMCOR0.WORD = PCLK / 8 / 1000 - 1;	/* Set clock divider */
	CMT.CMCSR0.WORD = 0x0040;		/* Enable interrupt, Clock source = PCLK/8 */
	CMT.CMSTR.BIT.STR0 = 1;			/* Start CMT0 */
	INTC.IPR10.BIT._CMT0 = 2;		/* Set interrupt level 2 */


	INTC.IBNR.WORD = 0;				/* Disable register bank feature */
	set_imask(0);					/* Enable interrrupt */
}



/*---------------------------------------------------------*/
/* User Provided Timer Function for FatFs module           */
/*---------------------------------------------------------*/
/* This is a real time clock service to be called from     */
/* FatFs module. Any valid time must be returned even if   */
/* the system does not support a real time clock.          */
/* This is not required in read-only configuration.        */

DWORD get_fattime (void)
{
	/* No RTC supprt. Return a fixed value 2010/4/26 0:00:00 */
	return	  ((DWORD)(2010 - 1980) << 25)	/* Y */
			| ((DWORD)4  << 21)				/* M */
			| ((DWORD)26 << 16)				/* D */
			| ((DWORD)0  << 11)				/* H */
			| ((DWORD)0  << 5)				/* M */
			| ((DWORD)0  >> 1);				/* S */
}



/*--------------------------------------------------------------------------*/
/* Monitor                                                                  */
/*--------------------------------------------------------------------------*/

static
int xstrlen (const char *str)
{
	int n = 0;

	while (*str++) n++;
	return n;
}


static
char *xstrcpy (char* dst, const char* src)
{
	char c, *d = dst;

	do {
		c = *src++;
		*d++ = c;
	} while (c);

	return dst;
}


static
void *xmemset (void *p, int c, int sz)
{
	char *pf = (char*)p;

	while (sz--) *pf++ = (char)c;
	return p;
}


static
char *xstrchr (const char *str, int c)
{
	while (*str) {
		if (*str == (char)c) return (char*)str;
		str++;
	}
	return 0;
}



static
FRESULT scan_files (	/* Scan directory contents */
	char* path			/* Pointer to the path name working buffer */
)
{
	DIR dirs;
	FRESULT res;
	BYTE i;
	char *fn;


	res = f_opendir(&dirs, path);	/* Open the direcotry */
	if (res == FR_OK) {
		i = xstrlen(path);
		while (((res = f_readdir(&dirs, &Finfo)) == FR_OK) && Finfo.fname[0]) {	/* Read an item */
			if (_FS_RPATH && Finfo.fname[0] == '.') continue;	/* Ignore dot entries */
#if _USE_LFN
			fn = *Finfo.lfname ? Finfo.lfname : Finfo.fname;	/* Use LFN if available */
#else
			fn = Finfo.fname;									/* Always use SFN */
#endif
			if (Finfo.fattrib & AM_DIR) {	/* Directory */
				AccDirs++;									/* Increment number of dirs */
				*(path+i) = '/'; xstrcpy(path+i+1, fn);		/* Scan in the dir */
				res = scan_files(path);
				*(path+i) = '\0';
				if (res != FR_OK) break;
			} else {						/* File */
			/*	xprintf("%s/%s\n", path, fn); */
				AccFiles++;								/* Increment number of files */
				AccSize += Finfo.fsize;					/* Add file size */
			}
		}
	}

	return res;
}



static
void put_rc (		/* Put FatFs return code */
	FRESULT rc
)
{
	const char *str =
		"OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
		"INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
		"INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0"
		"LOCKED\0" "NOT_ENOUGH_CORE\0" "TOO_MANY_OPEN_FILES\0";
	FRESULT i;

	for (i = 0; i != rc && *str; i++) {
		while (*str++) ;
	}
	xprintf("rc=%u FR_%s\n", (UINT)rc, str);
}



static
void show_disk_status (		/* Show physical disk status */
	BYTE drv
)
{
	DWORD dw;
	BYTE b, buf[64];
	WORD w;
	char *ty, *am;


	if (disk_ioctl(drv, GET_SECTOR_COUNT, &dw) == RES_OK)
		xprintf("Drive size: %lu sectors\n", dw);

	if (disk_ioctl(drv, GET_BLOCK_SIZE, &dw) == RES_OK)
		xprintf("Erase block size: %lu sectors\n", dw);

	if (disk_ioctl(drv, MMC_GET_TYPE, &b) == RES_OK) {
		ty = "Unknown"; am = "";
		if (b & CT_MMC) ty = "MMC";
		if (b & CT_SD1) ty = "SDv1";
		if (b & CT_SD2) {
			ty = "SDv2";
			am = (b & CT_BLOCK) ? "(Block)" : "(Byte)";
		}
		xprintf("Card type: %s%s\n", ty, am);
	}

	if (disk_ioctl(drv, MMC_GET_CSD, buf) == RES_OK)
		xputs("CSD:\n"); put_dump(buf, 0, 16, DW_CHAR);

	if (disk_ioctl(drv, MMC_GET_CID, buf) == RES_OK)
		xputs("CID:\n"); put_dump(buf, 0, 16, DW_CHAR);

	if (disk_ioctl(drv, MMC_GET_OCR, buf) == RES_OK)
		xputs("OCR:\n"); put_dump(buf, 0, 4, DW_CHAR);

	if (disk_ioctl(drv, MMC_GET_SDSTAT, buf) == RES_OK) {
		xputs("SD Status:\n");
		for (w = 0; w < 64; w += 16)
			put_dump(buf+w, w, 16, DW_CHAR);
	}
}




static
void show_fs_status (	/* Show volume status */
	char *path
)
{
	FRESULT res;
	DWORD dw;
	FATFS *fs;
	static const BYTE ft[] = {0, 12, 16, 32};


	res = f_getfree(path, &dw, &fs);
	if (res == FR_OK) {
		xprintf("FAT type = FAT%u\n", ft[fs->fs_type & 3]);
		xprintf("Bytes/Cluster = %lu\nNumber of FATs = %u\n", fs->csize * 512UL, fs->n_fats);
		if (fs->fs_type != FS_FAT32) xprintf("Root DIR entries = %u\n", fs->n_rootdir);
		xprintf("Sectors/FAT = %lu\nNumber of clusters = %lu\n", fs->fsize, fs->n_fatent - 2);
		xprintf("FAT start (lba) = %lu\nDIR start (lba,clustor) = %lu\nData start (lba) = %lu\n...",
				fs->fatbase, fs->dirbase, fs->database);

		AccSize = AccFiles = AccDirs = 0;
		res = scan_files(path);
		if (res == FR_OK) {
			xprintf("\r   \n%u files, %lu bytes.\n%u folders.\n"
					"%lu KB total disk space.\n%lu KB available.\n",
					AccFiles, AccSize, AccDirs,
					(fs->n_fatent - 2) * (fs->csize / 2), dw * (fs->csize / 2)
			);
		} else {
			 put_rc(res);
		}
	} else {
		 put_rc(res);
	}
}



/*--------------------------------------------------------------------------*/
/* Main                                                                     */
/*--------------------------------------------------------------------------*/

int main (void)
{
	char *ptr, *ptr2;
	long p1, p2, p3;
	BYTE res;
	UINT s1, s2, cnt, blen = sizeof(Buff);
	DWORD ofs = 0, sect = 0;
	FATFS *fs;


	IoInit();				/* Initialize peripherals */

	xdev_in(scif2_get);		/* Join console and SCIF2 */
	xdev_out(scif2_put);

	/* Startup message */
	delay_ms(10);
	xputs("\nFatFs module test monitor for SH7262\n");
	xputs(_USE_LFN ? "LFN Enabled" : "LFN Disabled");
	xprintf(", Code page: %u/ANSI\n", _CODE_PAGE);

#if _USE_LFN
	Finfo.lfname = Lfname;
	Finfo.lfsize = sizeof(Lfname);
#endif


	for (;;) {
		xputc('>');
		xgets(Line, sizeof(Line));

		ptr = Line;
		switch (*ptr++) {
		case '?' :	/* ? - Show usage */
			xputs(
				"md <address> [<len>] - Dump system memory\n"
				"\n"
				"dd [<lba>] - Dump secrtor\n"
				"di - Initialize disk\n"
				"ds - Show disk status\n"
				"\n"
				"bd <ofs> - Dump working buffer\n"
				"be <ofs> [<data>] ... - Edit working buffer\n"
				"br <lba> [<num>] - Read disk into working buffer\n"
				"bw <lba> [<num>] - Write working buffer into disk\n"
				"bf <val> - Fill working buffer\n"
				"\n"
				"fi - Force initialized the volume\n"
				"fs - Show volume status\n"
				"fl [<path>] - Show a directory\n"
				"fo <mode> <file> - Open a file\n"
				"fc - Close the file\n"
				"fe <ofs> - Move fp\n"
				"fd <len> - Read and dump the file\n"
				"fr <len> - Read the file\n"
				"fw <len> <val> - Write to the file\n"
				"fn <object name> <new name> - Rename an object\n"
				"fu <object name> - Unlink an object\n"
				"fv - Truncate the file at current fp\n"
				"fk <dir name> - Create a directory\n"
				"fa <atrr> <mask> <object name> - Change object attribute\n"
				"ft <year> <month> <day> <hour> <min> <sec> <object name> - Change timestamp of an object\n"
				"fx <src file> <dst file> - Copy a file\n"
				"fg <path> - Change current directory\n"
				"fm <rule> <cluster size> - Create file system\n"
				"fz [<len>] - Change R/W block length for fr/fw/fx command\n"
				"\n"
			);
			break;

		case 'm' :
			switch (*ptr++) {
			case 'd' :	/* md <address> [<count>] - Dump memory */
				if (!xatoi(&ptr, &p1)) break;
				if (!xatoi(&ptr, &p2)) p2 = 128;
				for (ptr=(char*)p1; p2 >= 16; ptr += 16, p2 -= 16)
					put_dump((BYTE*)ptr, (UINT)ptr, 16, DW_CHAR);
				if (p2) put_dump((BYTE*)ptr, (UINT)ptr, p2, DW_CHAR);
				break;
			}
			break;

		case 'd' :
			switch (*ptr++) {
			case 'd' :	/* dd [<lba>] - Dump secrtor */
				if (!xatoi(&ptr, &p2)) p2 = sect;
				res = disk_read(0, Buff, p2, 1);
				if (res) { xprintf("rc=%d\n", (WORD)res); break; }
				sect = p2 + 1;
				xprintf("Sector:%lu\n", p2);
				for (ptr=(char*)Buff, ofs = 0; ofs < 0x200; ptr+=16, ofs+=16)
					put_dump((BYTE*)ptr, ofs, 16, DW_CHAR);
				break;

			case 'i' :	/* di - Initialize disk */
				xprintf("rc=%d\n", (WORD)disk_initialize(0));
				break;

			case 's' :	/* ds - Show disk status */
				show_disk_status(0);
				break;
			}
			break;

		case 'b' :
			switch (*ptr++) {
			case 'd' :	/* bd <addr> - Dump R/W buffer */
				if (!xatoi(&ptr, &p1)) break;
				for (ptr=(char*)&Buff[p1], ofs = p1, cnt = 32; cnt; cnt--, ptr+=16, ofs+=16)
					put_dump((BYTE*)ptr, ofs, 16, DW_CHAR);
				break;

			case 'e' :	/* be <addr> [<data>] ... - Edit R/W buffer */
				if (!xatoi(&ptr, &p1)) break;
				if (xatoi(&ptr, &p2)) {
					do {
						Buff[p1++] = (BYTE)p2;
					} while (xatoi(&ptr, &p2));
					break;
				}
				for (;;) {
					xprintf("0x%04X: 0x%02X-", (WORD)(p1), (WORD)Buff[p1]);
					xgets(Line, sizeof(Line));
					ptr = Line;
					if (*ptr == '.') break;				/* Exit */
					if (*ptr < ' ') { p1++; continue; }	/* Skip */
					if (xatoi(&ptr, &p2))
						Buff[p1++] = (BYTE)p2;
					else
						xputs("???\n");
				}
				break;

			case 'r' :	/* br <lba> [<num>] - Read disk into R/W buffer */
				if (!xatoi(&ptr, &p2)) break;
				if (!xatoi(&ptr, &p3)) p3 = 1;
				xprintf("rc=%u\n", (WORD)disk_read(0, Buff, p2, p3));
				break;

			case 'w' :	/* bw <lba> [<num>] - Write R/W buffer into disk */
				if (!xatoi(&ptr, &p2)) break;
				if (!xatoi(&ptr, &p3)) p3 = 1;
				xprintf("rc=%u\n", (WORD)disk_write(0, Buff, p2, p3));
				break;

			case 'f' :	/* bf <val> - Fill working buffer */
				if (!xatoi(&ptr, &p1)) break;
				xmemset(Buff, (BYTE)p1, sizeof(Buff));
				break;

			}
			break;

		case 'f' :
			switch (*ptr++) {

			case 'i' :	/* fi - Force initialized the logical drive */
				put_rc(f_mount(0, &Fatfs));
				break;

			case 's' :	/* fs - Show logical drive status */
				ptr = Line;
				*ptr = 0;
				show_fs_status(ptr);
				break;

			case 'l' :	/* fl [<path>] - Directory listing */
				while (*ptr == ' ') ptr++;
				res = f_opendir(&Dir, ptr);
				if (res) { put_rc(res); break; }
				p1 = s1 = s2 = 0;
				for(;;) {
					res = f_readdir(&Dir, &Finfo);
					if ((res != FR_OK) || !Finfo.fname[0]) break;
					if (Finfo.fattrib & AM_DIR) {
						s2++;
					} else {
						s1++; p1 += Finfo.fsize;
					}
					xprintf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %-12s  %s\n",
							(Finfo.fattrib & AM_DIR) ? 'D' : '-',
							(Finfo.fattrib & AM_RDO) ? 'R' : '-',
							(Finfo.fattrib & AM_HID) ? 'H' : '-',
							(Finfo.fattrib & AM_SYS) ? 'S' : '-',
							(Finfo.fattrib & AM_ARC) ? 'A' : '-',
							(Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,
							(Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63,
							Finfo.fsize, Finfo.fname,
#if _USE_LFN
							Lfname);
#else
							"");
#endif
				}
				xprintf("%4u File(s),%10lu bytes total\n%4u Dir(s)", s1, p1, s2);
				if (f_getfree(ptr, (DWORD*)&p1, &fs) == FR_OK)
					xprintf(", %10lu bytes free\n", p1 * fs->csize * 512);
				break;

			case 'o' :	/* fo <mode> <file> - Open a file */
				if (!xatoi(&ptr, &p1)) break;
				while (*ptr == ' ') ptr++;
				put_rc(f_open(&File1, ptr, (BYTE)p1));
				break;

			case 'c' :	/* fc - Close a file */
				put_rc(f_close(&File1));
				break;

			case 'e' :	/* fe - Seek file pointer */
				if (!xatoi(&ptr, &p1)) break;
				res = f_lseek(&File1, p1);
				put_rc(res);
				if (res == FR_OK)
					xprintf("fptr=%lu(0x%lX)\n", File1.fptr, File1.fptr);
				break;

			case 'd' :	/* fd <len> - read and dump file from current fp */
				if (!xatoi(&ptr, &p1)) break;
				ofs = File1.fptr;
				while (p1) {
					if ((UINT)p1 >= 16) { cnt = 16; p1 -= 16; }
					else 				{ cnt = p1; p1 = 0; }
					res = f_read(&File1, Buff, cnt, &cnt);
					if (res != FR_OK) { put_rc(res); break; }
					if (!cnt) break;
					put_dump(Buff, ofs, cnt, DW_CHAR);
					ofs += 16;
				}
				break;

			case 'r' :	/* fr <len> - read file */
				if (!xatoi(&ptr, &p1)) break;
				p2 = 0;
				Timer = 0;
				while (p1) {
					if ((UINT)p1 >= blen) {
						cnt = blen; p1 -= blen;
					} else {
						cnt = p1; p1 = 0;
					}
					res = f_read(&File1, Buff, cnt, &s2);
					if (res != FR_OK) { put_rc(res); break; }
					p2 += s2;
					if (cnt != s2) break;
				}
				xprintf("%lu bytes read with %lu kB/sec.\n", p2, Timer ? (p2 / Timer) : 0);
				break;

			case 'w' :	/* fw <len> <val> - write file */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				xmemset(Buff, (BYTE)p2, blen);
				p2 = 0;
				Timer = 0;
				while (p1) {
					if ((UINT)p1 >= blen) {
						cnt = blen; p1 -= blen;
					} else {
						cnt = p1; p1 = 0;
					}
					res = f_write(&File1, Buff, cnt, &s2);
					if (res != FR_OK) { put_rc(res); break; }
					p2 += s2;
					if (cnt != s2) break;
				}
				xprintf("%lu bytes written with %lu kB/sec.\n", p2, Timer ? (p2 / Timer) : 0);
				break;

			case 'n' :	/* fn <old_name> <new_name> - Change file/dir name */
				while (*ptr == ' ') ptr++;
				ptr2 = xstrchr(ptr, ' ');
				if (!ptr2) break;
				*ptr2++ = 0;
				while (*ptr2 == ' ') ptr2++;
				put_rc(f_rename(ptr, ptr2));
				break;

			case 'u' :	/* fu <name> - Unlink a file or dir */
				while (*ptr == ' ') ptr++;
				put_rc(f_unlink(ptr));
				break;

			case 'v' :	/* fv - Truncate file */
				put_rc(f_truncate(&File1));
				break;

			case 'k' :	/* fk <name> - Create a directory */
				while (*ptr == ' ') ptr++;
				put_rc(f_mkdir(ptr));
				break;

			case 'a' :	/* fa <atrr> <mask> <name> - Change file/dir attribute */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				while (*ptr == ' ') ptr++;
				put_rc(f_chmod(ptr, p1, p2));
				break;

			case 't' :	/* ft <year> <month> <day> <hour> <min> <sec> <name> - Change timestamp */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				Finfo.fdate = ((p1 - 1980) << 9) | ((p2 & 15) << 5) | (p3 & 31);
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				Finfo.ftime = ((p1 & 31) << 11) | ((p2 & 63) << 5) | ((p3 >> 1) & 31);
				put_rc(f_utime(ptr, &Finfo));
				break;

			case 'x' : /* fx <src_name> <dst_name> - Copy file */
				while (*ptr == ' ') ptr++;
				ptr2 = xstrchr(ptr, ' ');
				if (!ptr2) break;
				*ptr2++ = 0;
				while (*ptr2 == ' ') ptr2++;
				xprintf("Opening \"%s\"", ptr);
				res = f_open(&File1, ptr, FA_OPEN_EXISTING | FA_READ);
				xputc('\n');
				if (res) {
					put_rc(res);
					break;
				}
				xprintf("Creating \"%s\"", ptr2);
				res = f_open(&File2, ptr2, FA_CREATE_ALWAYS | FA_WRITE);
				xputc('\n');
				if (res) {
					put_rc(res);
					f_close(&File1);
					break;
				}
				xprintf("Copying file...");
				Timer = 0;
				p1 = 0;
				for (;;) {
					res = f_read(&File1, Buff, blen, &s1);
					if (res || s1 == 0) break;   /* error or eof */
					res = f_write(&File2, Buff, s1, &s2);
					p1 += s2;
					if (res || s2 < s1) break;   /* error or disk full */
				}
				xputc('\n');
				if (res)
					put_rc(res);
				else
					xprintf("%lu bytes copied with %lu kB/sec.\n", p1, p1 / Timer);
				f_close(&File1);
				f_close(&File2);
				break;
#if _FS_RPATH >= 1
			case 'g' :	/* fg <path> - Change current directory */
				while (*ptr == ' ') ptr++;
				put_rc(f_chdir(ptr));
				break;
#if _FS_RPATH >= 2
			case 'q' :	/* fq - Show current dir path */
				res = f_getcwd(Line, sizeof(Line));
				if (res)
					put_rc(res);
				else
					xprintf("%s\n", Line);
				break;
#endif
#endif
#if _USE_MKFS
			case 'm' :	/* fm <partition rule> <cluster size> - Create file system */
				if (!xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				xprintf("The card will be formatted. Are you sure? (Y/n)=");
				xgets(ptr, sizeof(Line));
				if (*ptr == 'Y')
					put_rc(f_mkfs(0, (BYTE)p2, (WORD)p3));
				break;
#endif
			case 'z' :	/* fz [<rw size>] - Change R/W length for fr/fw/fx command */
				if (xatoi(&ptr, &p1) && p1 >= 1 && p1 <= sizeof(Buff))
					blen = p1;
				xprintf("blen=%u\n", blen);
				break;
			}
			break;

		}
	}
}




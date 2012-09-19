/*----------------------------------------------------------------------*/
/* FatFs Module Sample Program / Renesas RX62N        (C)ChaN, 2011     */
/*----------------------------------------------------------------------*/
/* Ev.Board: FRK-RX62N from CQ Publishing                               */
/* Console: N81 115200bps (port is defined in scif.c)                   */
/* MMC/SDC: SPI mode (port is defined in mmc_rspi.c)                    */
/*----------------------------------------------------------------------*/


#include <machine.h>
#include "iodefine.h"
#include "vect.h"
#include "integer.h"
#include "scif.h"
#include "xprintf.h"
#include "diskio.h"
#include "ff.h"
#include "sound.h"

#define	F_PCLK	96000000UL

extern void disk_timerproc (void);


/*---------------------------------------------------------*/
/* Work Area                                               */
/*---------------------------------------------------------*/

FATFS Fatfs;
FIL File1, File2;
DIR Dir;

DWORD acc_size;				/* File counter (fs command) */
WORD acc_files, acc_dirs;

FILINFO Finfo;				/* File properties (fs/fl command) */
#if _USE_LFN
char Lfname[_MAX_LFN + 1];
#endif

char Line[256];				/* Console input buffer */
BYTE Buff[16384];			/* Disk I/O working buffer */

volatile UINT Timer;		/* Performance timer (1kHz) */




/*---------------------------------------------------------*/
/* 1000Hz interval timer (CMT0)                            */
/*---------------------------------------------------------*/

void Excep_CMTU0_CMT0(void)		/* ISR: vect.h is required */
{
	static WORD b;


	/* Increment performance timer */
	Timer++;

	/* Blink LED1 on the board */
	b++;
	PORT1.DR.BIT.B5 = ((b & 0x40) && (b & 0x680) == 0) ? 0 : 1;

	/* Drive MMC/SD control module (mmc_rspi.c) */
	disk_timerproc();
}


void delay_ms (					/* Delay in unit of msec */
	UINT ms
)
{
	for (Timer = 0; Timer < ms; ) ;
}



/*---------------------------------------------------------*/
/* Initialize clock system and start timer service         */
/*---------------------------------------------------------*/


static
void clock_init (void)
{
	SYSTEM.SCKCR.LONG = 0x00010000;		/* Select system clock: ICLK=8x(96MHz), BCLK=4x(48MHz), PCLK=8x(96MHz) */

	/* Initialize CMT0 (1kHz IVT) */
	MSTP_CMT0 = 0;						/* Enable CMT0/1 module */
	CMT0.CMCR.WORD = 0x0040;			/* CMIE=1, CKS=0(PCLK/8) */
	CMT0.CMCOR = F_PCLK / 8 / 1000 - 1;	/* Select clock divider */
	CMT.CMSTR0.BIT.STR0 = 1;			/* Start CMT0 */
	IPR(CMT0, CMI0) = 8;				/* Interrupt priority = 8 */
	IEN(CMT0, CMI0) = 1;				/* Enable CMT0 interrupt */
}



/*---------------------------------------------------------*/
/* User Provided RTC Function for FatFs module             */
/*---------------------------------------------------------*/
/* This is a real time clock service to be called from     */
/* FatFs module. Any valid time must be returned even if   */
/* the system does not support an RTC.                     */
/* This function is not required in read-only cfg.         */

DWORD get_fattime (void)
{
	/* No RTC feature provided. Return a fixed value 2011/1/29 0:00:00 */
	return	  ((DWORD)(2011 - 1980) << 25)	/* Y */
			| ((DWORD)1  << 21)				/* M */
			| ((DWORD)29 << 16)				/* D */
			| ((DWORD)0  << 11)				/* H */
			| ((DWORD)0  << 5)				/* M */
			| ((DWORD)0  >> 1);				/* S */
}



/*--------------------------------------------------------------------------*/
/* Monitor                                                                  */
/*--------------------------------------------------------------------------*/


int xstrlen (const char *str)
{
	int n = 0;

	while (*str++) n++;
	return n;
}


char *xstrcpy (char* dst, const char* src)
{
	char c, *d = dst;

	do {
		c = *src++;
		*d++ = c;
	} while (c);

	return dst;
}


void *xmemset (void *p, int c, int sz)
{
	char *pf = (char*)p;

	while (sz--) *pf++ = (char)c;
	return p;
}


char *xstrchr (const char *str, int c)
{
	while (*str) {
		if (*str == (char)c) return (char*)str;
		str++;
	}
	return 0;
}


static
FRESULT scan_files (	/* Scan directory in recursive */
	char* path			/* Pointer to the path name working buffer */
)
{
	DIR dirs;
	FRESULT fr;
	BYTE i;
	char *fn;


	fr = f_opendir(&dirs, path);	/* Open the directory */
	if (fr == FR_OK) {
		i = xstrlen(path);
		while (((fr = f_readdir(&dirs, &Finfo)) == FR_OK) && Finfo.fname[0]) {	/* Get an entry from the dir */
			if (_FS_RPATH && Finfo.fname[0] == '.') continue;	/* Ignore dot entry */
#if _USE_LFN
			fn = *Finfo.lfname ? Finfo.lfname : Finfo.fname;	/* Use LFN if available */
#else
			fn = Finfo.fname;	/* Always use SFN under non-LFN cgf. */
#endif
			if (Finfo.fattrib & AM_DIR) {	/* It is a directory */
				acc_dirs++;
				*(path+i) = '/'; xstrcpy(path+i+1, fn);	/* Scan the directory */
				fr = scan_files(path);
				*(path+i) = '\0';
				if (fr != FR_OK) break;
			} else {						/* It is a file  */
			/*	xprintf("%s/%s\n", path, fn); */
				acc_files++;
				acc_size += Finfo.fsize;				/* Accumulate the file size in unit of byte */
			}
		}
	}

	return fr;
}




static
void put_rc (		/* Put FatFs result code with defined symbol */
	FRESULT rc
)
{
	const char *str =
		"OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
		"INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
		"INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0"
		"LOCKED\0" "NOT_ENOUGH_CORE\0" "TOO_MANY_OPEN_FILES\0";
	FRESULT i;

	for (i = FR_OK; i != rc && *str; i++) {
		while (*str++) ;
	}
	xprintf("rc=%u FR_%s\n", (UINT)rc, str);
}


static
char HelpMsg[] = {
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
	"fq - Show current dir path\n"
	"fm <rule> <cluster size> - Create file system\n"
	"fz [<len>] - Change R/W block length for fr/fw/fx command\n"
	"\n"
};



/*--------------------------------------------------------------------------*/

int main (void)
{
	char *ptr, *ptr2;
	long p1, p2, p3;
	BYTE dr, b1;
	FRESULT fr;
	UINT s1, s2, cnt, blen = sizeof Buff;
	DWORD ofs = 0, sect = 0;
	FATFS *fs;
	static const BYTE ft[] = {0,12,16,32};


	PORT1.DDR.BIT.B5 = 1;	/* P15:LED ON */

	clock_init();			/* Initialize clock and timer */

	scif_init(115200);		/* Initialize serial port (N81 115.2kbps) */
	xdev_in(scif_get);		/* Join scif.c and monitor.c */
	xdev_out(scif_put);

	delay_ms(10);
	xputs("\nFatFs module test monitor for FRK-RN62N evaluation board\n");
	xputs(_USE_LFN ? "LFN Enabled" : "LFN Disabled");
	xprintf(", Code page: %u/ANSI\n", _CODE_PAGE);

#if _USE_LFN
	Finfo.lfname = Lfname;
	Finfo.lfsize = sizeof Lfname;
#endif

	for (;;) {
		xputc('>');
		xgets(Line, sizeof Line);

		ptr = Line;
		switch (*ptr++) {
		case '?' :		/* ? - Show usage */
			xputs(HelpMsg);
			break;

		case 'm' :	/* Memory dump/fill/edit */
			switch (*ptr++) {
			case 'd' :	/* md[b|h|w] <address> [<count>] - Dump memory */
				switch (*ptr++) {
				case 'w': p3 = DW_LONG; break;
				case 'h': p3 = DW_SHORT; break;
				default: p3 = DW_CHAR;
				}
				if (!xatoi(&ptr, &p1)) break;
				if (!xatoi(&ptr, &p2)) p2 = 128 / p3;
				for (ptr = (char*)p1; p2 >= 16 / p3; ptr += 16, p2 -= 16 / p3)
					put_dump(ptr, (DWORD)ptr, 16 / p3, p3);
				if (p2) put_dump((BYTE*)ptr, (UINT)ptr, p2, p3);
				break;
			case 'f' :	/* mf <address> <byte> <count> - Fill memory */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				while (p3--) {
					*(BYTE*)p1 = (BYTE)p2;
					p1++;
				}
				break;
			case 'e' :	/* me[b|h|w] <address> [<value> ...] - Edit memory */
				switch (*ptr++) {	/* Get data width */
				case 'w': p3 = 4; break;
				case 'h': p3 = 2; break;
				default: p3 = 1;
				}
				if (!xatoi(&ptr, &p1)) break;	/* Get start address */
				if (xatoi(&ptr, &p2)) {	/* 2nd parameter is given (direct mode) */
					do {
						switch (p3) {
						case 4: *(DWORD*)p1 = (DWORD)p2; break;
						case 2: *(WORD*)p1 = (WORD)p2; break;
						default: *(BYTE*)p1 = (BYTE)p2;
						}
						p1 += p3;
					} while (xatoi(&ptr, &p2));	/* Get next value */
					break;
				}
				for (;;) {				/* 2nd parameter is not given (interactive mode) */
					switch (p3) {
					case 4: xprintf("%08X 0x%08X-", p1, *(DWORD*)p1); break;
					case 2: xprintf("%08X 0x%04X-", p1, *(WORD*)p1); break;
					default: xprintf("%08X 0x%02X-", p1, *(BYTE*)p1);
					}
					ptr = Line; xgets(ptr, sizeof Line);
					if (*ptr == '.') break;
					if ((BYTE)*ptr >= ' ') {
						if (!xatoi(&ptr, &p2)) continue;
						switch (p3) {
						case 4: *(DWORD*)p1 = (DWORD)p2; break;
						case 2: *(WORD*)p1 = (WORD)p2; break;
						default: *(BYTE*)p1 = (BYTE)p2;
						}
					}
					p1 += p3;
				}
				break;
			}
			break;

		case 'd' :
			switch (*ptr++) {
			case 'd' :	/* dd [<lba>] - Dump secrtor */
				if (!xatoi(&ptr, &p2)) p2 = sect;
				dr = disk_read(0, Buff, p2, 1);
				if (dr) { xprintf("rc=%d\n", (WORD)dr); break; }
				sect = p2 + 1;
				xprintf("Sector:%lu\n", p2);
				for (ptr=(char*)Buff, ofs = 0; ofs < 0x200; ptr+=16, ofs+=16)
					put_dump((BYTE*)ptr, ofs, 16, DW_CHAR);
				break;

			case 'i' :	/* di - Initialize disk */
				xprintf("rc=%d\n", (WORD)disk_initialize(0));
				break;

			case 's' :	/* ds - Show disk status */
				if (disk_ioctl(0, GET_SECTOR_COUNT, &p2) == RES_OK)
					{ xprintf("Drive size: %lu sectors\n", p2); }
				if (disk_ioctl(0, GET_BLOCK_SIZE, &p2) == RES_OK)
					{ xprintf("Erase block size: %lu sectors\n", p2); }
				if (disk_ioctl(0, MMC_GET_TYPE, &b1) == RES_OK)
					{ xprintf("MMC/SDC type: %u\n", b1); }
				if (disk_ioctl(0, MMC_GET_CSD, Buff) == RES_OK)
					{ xputs("CSD:\n"); put_dump(Buff, 0, 16, DW_CHAR); }
				if (disk_ioctl(0, MMC_GET_CID, Buff) == RES_OK)
					{ xputs("CID:\n"); put_dump(Buff, 0, 16, DW_CHAR); }
				if (disk_ioctl(0, MMC_GET_OCR, Buff) == RES_OK)
					{ xputs("OCR:\n"); put_dump(Buff, 0, 4, DW_CHAR); }
				if (disk_ioctl(0, MMC_GET_SDSTAT, Buff) == RES_OK) {
					xputs("SD Status:\n");
					for (s1 = 0; s1 < 64; s1 += 16) put_dump(Buff+s1, s1, 16, DW_CHAR);
				}
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
					xgets(Line, sizeof Line);
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
				xmemset(Buff, (BYTE)p1, sizeof Buff);
				break;

			}
			break;

		case 'f' :
			switch (*ptr++) {

			case 'i' :	/* fi - Force initialized the logical drive */
				put_rc(f_mount(0, &Fatfs));
				break;

			case 's' :	/* fs - Show logical drive status */
				while (*ptr == ' ') ptr++;
				/* Volume status */
				fr = f_getfree(ptr, (DWORD*)&p2, &fs);
				if (fr) { put_rc(fr); break; }
				xprintf("FAT type = FAT%u\nBytes/Cluster = %lu\nNumber of FATs = %u\n"
						"Root DIR entries = %u\nSectors/FAT = %lu\nNumber of clusters = %lu\n"
						"FAT start (lba) = %lu\nDIR start (lba,clustor) = %lu\nData start (lba) = %lu\n\n...",
						ft[fs->fs_type & 3], (DWORD)fs->csize * 512, (WORD)fs->n_fats,
						fs->n_rootdir, fs->fsize, (DWORD)fs->n_fatent - 2,
						fs->fatbase, fs->dirbase, fs->database
				);
				/* Volume contents */
				acc_size = acc_files = acc_dirs = 0;
#if _USE_LFN
				Finfo.lfname = Lfname;
				Finfo.lfsize = sizeof Lfname;
#endif
				fr = scan_files(ptr);
				if (fr) { put_rc(fr); break; }
				xprintf("\r%u files, %lu bytes.\n%u folders.\n"
						"%lu KB total disk space.\n%lu KB available.\n",
						acc_files, acc_size, acc_dirs,
						(fs->n_fatent - 2) * (fs->csize / 2), p2 * (fs->csize / 2)
				);
				break;

			case 'l' :	/* fl [<path>] - Directory listing */
				while (*ptr == ' ') ptr++;
				fr = f_opendir(&Dir, ptr);
				if (fr) { put_rc(fr); break; }
				p1 = s1 = s2 = 0;
				for(;;) {
					fr = f_readdir(&Dir, &Finfo);
					if ((fr != FR_OK) || !Finfo.fname[0]) break;
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
				fr = f_lseek(&File1, p1);
				put_rc(fr);
				if (fr == FR_OK)
					xprintf("fptr=%lu(0x%lX)\n", File1.fptr, File1.fptr);
				break;

			case 'd' :	/* fd <len> - read and dump file from current fp */
				if (!xatoi(&ptr, &p1)) break;
				ofs = File1.fptr;
				while (p1) {
					if ((UINT)p1 >= 16) { cnt = 16; p1 -= 16; }
					else 				{ cnt = p1; p1 = 0; }
					fr = f_read(&File1, Buff, cnt, &cnt);
					if (fr != FR_OK) { put_rc(fr); break; }
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
					fr = f_read(&File1, Buff, cnt, &s2);
					if (fr != FR_OK) { put_rc(fr); break; }
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
					fr = f_write(&File1, Buff, cnt, &s2);
					if (fr != FR_OK) { put_rc(fr); break; }
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
				fr = f_open(&File1, ptr, FA_OPEN_EXISTING | FA_READ);
				xputc('\n');
				if (fr) {
					put_rc(fr);
					break;
				}
				xprintf("Creating \"%s\"", ptr2);
				fr = f_open(&File2, ptr2, FA_CREATE_ALWAYS | FA_WRITE);
				xputc('\n');
				if (fr) {
					put_rc(fr);
					f_close(&File1);
					break;
				}
				xprintf("Copying file...");
				Timer = 0;
				p1 = 0;
				for (;;) {
					fr = f_read(&File1, Buff, blen, &s1);
					if (fr || s1 == 0) break;   /* error or eof */
					fr = f_write(&File2, Buff, s1, &s2);
					p1 += s2;
					if (fr || s2 < s1) break;   /* error or disk full */
				}
				xprintf("%lu bytes copied with %lu kB/sec.\n", p1, p1 / Timer);
				f_close(&File1);
				f_close(&File2);
				break;
#if _FS_RPATH
			case 'g' :	/* fg <path> - Change current directory */
				while (*ptr == ' ') ptr++;
				put_rc(f_chdir(ptr));
				break;
#if _FS_RPATH >= 2
			case 'q' :	/* fq - Show current dir path */
				fr = f_getcwd(Line, sizeof Line);
				if (fr)
					put_rc(fr);
				else
					xprintf("%s\n", Line);
				break;
#endif
#endif
#if _USE_MKFS
			case 'm' :	/* fm <partition rule> <cluster size> - Create file system */
				if (!xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				xprintf("The card will be formatted. Are you sure? (Y/n)=");
				xgets(ptr, sizeof Line);
				if (*ptr == 'Y')
					put_rc(f_mkfs(0, (BYTE)p2, (WORD)p3));
				break;
#endif
			case 'z' :	/* fz [<rw size>] - Change R/W length for fr/fw/fx command */
				if (xatoi(&ptr, &p1) && p1 >= 1 && p1 <= sizeof Buff)
					blen = p1;
				xprintf("blen=%u\n", blen);
				break;
			}
			break;
#ifdef SOUND_DEFINED
		case 'p' :	/* p <wavfile> - Play RIFF-WAV file */
			while (*ptr == ' ') ptr++;
			fr = f_open(&File1, ptr, FA_READ);
			if (fr) {
				put_rc(fr);
			} else {
				load_wav(&File1, ptr, Buff, sizeof Buff);
				f_close(&File1);
			}
			break;
#endif

		}
	}
}



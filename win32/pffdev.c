/*---------------------------------------------------------------*/
/* Petit FAT file system module test program R0.02 (C)ChaN, 2010 */
/*---------------------------------------------------------------*/


#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "monitor.h"
#include "diskio.h"
#include "pff.h"


/*---------------------------------------------------------*/
/* Work Area                                               */
/*---------------------------------------------------------*/


char linebuf[128];		/* Console input buffer */

BYTE Buff[1024];		/* Working buffer */



void put_rc (FRESULT rc)
{
	const char *p;
	static const char str[] =
		"OK\0" "DISK_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
		"NOT_OPENED\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0";
	FRESULT i;

	for (p = str, i = 0; i != rc && *p; i++) {
		while(*p++);
	}
	printf("rc=%u FR_%s\n", (UINT)rc, p);
}





/*-----------------------------------------------------------------------*/
/* Main                                                                  */


int main (int argc, char *argv[])
{
	char *ptr;
	long p1, p2, p3;
	BYTE res;
	UINT s1, s2, ofs;
	WORD w, cnt;
	FATFS fs;			/* File system object */
	DIR dir;			/* Directory object */
	FILINFO fno;		/* File information */


	if (!assign_drives(argc, argv)) {
		printf("\nUsage: ffdev <phy drv#> [<phy drv#>] ...\n");
		return 2;
	}

	printf("\nPetit FatFs module test monitor on Win32\n");

	for (;;) {
		printf(">");
		gets(ptr = linebuf);

		switch (*ptr++) {

		case 'q' :	/* Exit program */
			return 0;

		case 'T' :
			while (*ptr == ' ') ptr++;

			// Quick test space

			break;

		case 'd' :
			switch (*ptr++) {
			case 'd' :	/* dd <sector> <ofs> <count> - Dump secrtor */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				p2 %= 512;
				res = disk_readp(Buff, p1, (WORD)p2, (WORD)p3);
				if (res) { printf("rc=%d\n", (WORD)res); break; }
				printf("Sector:%lu\n", p1);
				p3 = p2 + p3;
				for (ptr=(char*)Buff; p2 < p3; ptr+=16, p2+=16) {
					if (p3 - p2 >= 16) 
						put_dump((BYTE*)ptr, p2, 16);
					else
						put_dump((BYTE*)ptr, p2, p3 - p2);
				}
				break;

			case 'w' :	/* dw <sector> <count> <dat> - Write sector */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
				memset(Buff, (BYTE)p3, 512);
				res = disk_writep(0, p1);
				if (res) { printf("rc=%d\n", res); break; }
				res = disk_writep(Buff, p2);
				if (res) { printf("rc=%d\n", res); break; }
				res = disk_writep(0, 0);
				if (res) { printf("rc=%d\n", res); break; }
				break;

			case 'i' :	/* di - Initialize physical drive */
				res = disk_initialize();
				printf("rc=%d\n", res);
				break;
			}
			break;

		case 'f' :
			switch (*ptr++) {

			case 'i' :	/* fi - Mount the volume */
				put_rc(pf_mount(&fs));
				break;

			case 's' :	/* fs - Show logical drive status */
				if (!fs.fs_type) { printf("Not mounted.\n"); break; }
				printf("FAT type = %u\nBytes/Cluster = %lu\n"
						"Root DIR entries = %u\nNumber of clusters = %lu\n"
						"FAT start (lba) = %lu\nDIR start (lba,clustor) = %lu\nData start (lba) = %lu\n\n",
						(WORD)fs.fs_type,
						(DWORD)fs.csize * 512,
						fs.n_rootdir, (DWORD)fs.n_fatent - 2,
						fs.fatbase, fs.dirbase, fs.database
				);
				break;

			case 'l' :	/* fl [<path>] - Directory listing */
				res = pf_opendir(&dir, ptr);
				if (res) { put_rc(res); break; }
				p1 = s1 = s2 = 0;
				for(;;) {
					res = pf_readdir(&dir, &fno);
					if (res != FR_OK) { put_rc(res); break; }
					if (!fno.fname[0]) break;
					if (fno.fattrib & AM_DIR) {
						s2++;
					} else {
						s1++; p1 += fno.fsize;
					}
					printf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %s",
							(fno.fattrib & AM_DIR) ? 'D' : '-',
							(fno.fattrib & AM_RDO) ? 'R' : '-',
							(fno.fattrib & AM_HID) ? 'H' : '-',
							(fno.fattrib & AM_SYS) ? 'S' : '-',
							(fno.fattrib & AM_ARC) ? 'A' : '-',
							(fno.fdate >> 9) + 1980, (fno.fdate >> 5) & 15, fno.fdate & 31,
							(fno.ftime >> 11), (fno.ftime >> 5) & 63, fno.fsize, fno.fname);
					putchar('\n');
				}
				printf("%4u File(s),%10lu bytes total\n%4u Dir(s)\n", s1, p1, s2);
				break;

			case 'o' :	/* fo <file> - Open a file */
				put_rc(pf_open(ptr));
				break;

			case 'e' :	/* fe - Move file pointer of the file */
				if (!xatoi(&ptr, &p1)) break;
				res = pf_lseek(p1);
				put_rc(res);
				if (res == FR_OK)
					printf("fptr = %lu(0x%lX)\n", fs.fptr, fs.fptr);
				break;

			case 'r' :	/* fr <len> - Read the file */
				if (!xatoi(&ptr, &p1)) break;
				p2 =0;
				while (p1) {
					if ((UINT)p1 >= sizeof(Buff)) {
						cnt = sizeof(Buff); p1 -= sizeof(Buff);
					} else {
						cnt = (WORD)p1; p1 = 0;
					}
					res = pf_read(Buff, cnt, &w);
					if (res != FR_OK) { put_rc(res); break; }
					p2 += w;
					if (cnt != w) break;
				}
				printf("%lu bytes read.\n", p2);
				break;

			case 'w' :	/* fw <len> <dat> - Read the file */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				memset(Buff, (BYTE)p2, sizeof(Buff));
				p3 =0;
				while (p1) {
					if ((UINT)p1 >= sizeof(Buff)) {
						cnt = sizeof(Buff); p1 -= sizeof(Buff);
					} else {
						cnt = (WORD)p1; p1 = 0;
					}
					res = pf_write(Buff, cnt, &w);
					if (res != FR_OK) { put_rc(res); break; }
					p3 += w;
					if (cnt != w) break;
				}
				res = pf_write(0, 0, &w);
				if (res != FR_OK) { put_rc(res); break; }
				printf("%lu bytes written.\n", p3);
				break;

			case 'd' :	/* fd <len> - Read and dump the file */
				if (!xatoi(&ptr, &p1)) break;
				ofs = fs.fptr;
				while (p1) {
					if ((UINT)p1 >= 16) { cnt = 16; p1 -= 16; }
					else 				{ cnt = (WORD)p1; p1 = 0; }
					res = pf_read(Buff, cnt, &w);
					if (res != FR_OK) { put_rc(res); break; }
					if (!w) break;
					put_dump(Buff, ofs, cnt);
					ofs += 16;
				}
				break;

			case 't' :	/* ft - Type the file via streaming function */
				do {
					res = pf_read(0, 32768, &w);
					if (res != FR_OK) { put_rc(res); break; }
				} while (w == 32768);
				break;

			}
			break;

		}
	}

}



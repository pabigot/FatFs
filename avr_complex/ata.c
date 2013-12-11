/*-----------------------------------------------------------------------*/
/* ATA harddisk control module                            (C)ChaN, 2013  */
/*-----------------------------------------------------------------------*/

#define N_DRIVES	1	/* 1:Master only, 2:Master+Slave */


#include <avr/io.h>
#include <avr/interrupt.h>
#include "diskio.h"
#include "xitoa.h"


/* Contorl Ports */
#define	CTRL_PORT		PORTA
#define	CTRL_DDR		DDRA
#define	DATH_PORT		PORTC
#define	DATH_DDR		DDRC
#define	DATH_PIN		PINC
#define	DATL_PORT		PORTD
#define	DATL_DDR		DDRD
#define	DATL_PIN		PIND

/* Bit definitions for Control Port */
#define	REG_DATA		0b11110000	/* Select Data register */
#define	REG_ERROR		0b11110001	/* Select Error register */
#define	REG_FEATURES	0b11110001	/* Select Features register */
#define	REG_COUNT		0b11110010	/* Select Count register */
#define	REG_SECTOR		0b11110011	/* Select Sector register */
#define	REG_CYLL		0b11110100	/* Select Cylinder low register */
#define	REG_CYLH		0b11110101	/* Select Cylinder high regitser */
#define	REG_DEV			0b11110110	/* Select Device register */
#define	REG_COMMAND		0b11110111	/* Select Command register */
#define	REG_STATUS		0b11110111	/* Select Status register */
#define	REG_DEVCTRL		0b11101110	/* Select Device control register */
#define	REG_ALTSTAT		0b11101110	/* Select Alternative Setatus register */
#define	IORD			0b00100000	/* IORD# bit in the control port */
#define	IOWR			0b01000000	/* IOWR# bit in the control port */
#define	RESET			0b10000000	/* RESE# bit in the control port */

/* ATA command */
#define CMD_READ		0x20	/* READ SECTOR(S) */
#define CMD_WRITE		0x30	/* WRITE SECTOR(S) */
#define CMD_IDENTIFY	0xEC	/* DEVICE IDENTIFY */
#define CMD_SETFEATURES	0xEF	/* SET FEATURES */

/* ATA register bit definitions */
#define	LBA				0x40	/* REG_DEV */
#define	DEV				0x10	/* REG_DEV */
#define	BSY				0x80	/* REG_STATUS */
#define	DRDY			0x40	/* REG_STATUS */
#define	DRQ				0x08	/* REG_STATUS */
#define	ERR				0x01	/* REG_STATUS */
#define	SRST			0x04	/* REG_DEVCTRL */
#define	NIEN			0x02	/* REG_DEVCTRL */


/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/


static
DSTATUS Stat[2] = {STA_NOINIT, STA_NOINIT};	/* Disk status */

static
BYTE Init;	/* b0:master initialized, b1:slave initialized */

static
volatile UINT Timer;	/* 100Hz decrement timer */



static
void set_timer (
	UINT ms
)
{
	ms /= 10;
	cli();
	Timer = ms;
	sei();
}


static
UINT get_timer (void)
{
	UINT n;


	cli();
	n = Timer;
	sei();

	return n * 10;
}


static
void delay_ms (
	UINT ms
)
{
	ms /= 10;
	cli(); Timer = ms; sei();

	do {
		cli(); ms = Timer; sei();
	} while (ms);
}



/*-----------------------------------------------------------------------*/
/* Initialize control port (Platform dependent)                          */
/*-----------------------------------------------------------------------*/

static
void init_port (void)
{
	DATL_PORT = 0x7F; DATH_PORT = 0xFF;	/* Set D15..D0 as input (pull-up) */
	DATL_DDR = 0; DATH_DDR = 0;

	CTRL_PORT = IORD | IOWR;	/* Set controls as output and assert RESET# */
	CTRL_DDR = 0xFF;
	delay_ms(20);

	CTRL_PORT |= RESET;			/* Deassert RESET# */
	delay_ms(20);
}



/*-----------------------------------------------------------------------*/
/* Read an ATA register (Platform dependent)                             */
/*-----------------------------------------------------------------------*/

/* Read ATA register */
static
BYTE read_ata (
	BYTE reg			/* Register to read */
)
{
	BYTE rd;


	cli();
	CTRL_PORT = reg;		/* Set register address [CS1#,CS0#,A2..A0] */
	CTRL_PORT &= ~IORD;		/* IORD# = L */
	CTRL_PORT; CTRL_PORT;	/* Delay */
	rd = DATL_PIN;			/* Read data */
	CTRL_PORT |= IORD;		/* IORD# = H */
	sei();

	return rd;
}


/* Read 512 bytes from ATA data register but store a part of block */
static
void read_block (
	BYTE *buf
)
{
	BYTE dl, dh, c, iord_l, iord_h;


	CTRL_PORT = REG_DATA;		/* Select data register */
	iord_h = REG_DATA;
	iord_l = REG_DATA & ~IORD;
	c = 128;
	cli();
	do {	/* Receive 4 bytes/loop */
		CTRL_PORT = iord_l;		/* IORD# = L */
		CTRL_PORT; CTRL_PORT;	/* Delay */
		dl = DATL_PIN; dh = DATH_PIN;	/* Read data on the D15..D0 */
		CTRL_PORT = iord_h;		/* IORD# = H */
		CTRL_PORT = iord_l;		/* IORD# = L */
		*buf++ = dl; *buf++ = dh;	/* Store data (delay) */
		dl = DATL_PIN; dh = DATH_PIN;	/* Read data on the D15..D0 */
		CTRL_PORT = iord_h;		/* IORD = H */
		*buf++ = dl; *buf++ = dh;	/* Store data */
	} while (--c);
	sei();
}


/* Read 512 bytes from ATA data register but store a part of block */
static
void read_block_part (
	BYTE *buf,
	BYTE ofs,
	BYTE nw
)
{
	BYTE c, dl, dh;


	CTRL_PORT = REG_DATA;		/* Select Data register */
	c = 0;
	cli();
	do {
		CTRL_PORT &= ~IORD;		/* IORD# = L */
		CTRL_PORT; CTRL_PORT;	/* Delay */
		dl = DATL_PIN;			/* Read even byte */
		dh = DATH_PIN;			/* Read odd byte */
		CTRL_PORT |= IORD;		/* IORD# = H */
		if (nw && (c >= ofs)) {	/* Pick up a part of block */
			*buf++ = dl; *buf++ = dh;
			nw--;
		}
	} while (++c);
	sei();
}



/*-----------------------------------------------------------------------*/
/* Write a byte to an ATA register (Platform dependent)                  */
/*-----------------------------------------------------------------------*/

/* Write a byte to the ATA register */
static
void write_ata (
	BYTE reg,		/* Register to be written */
	BYTE dat		/* Data to be written */
)
{
	cli();
	CTRL_PORT = reg;		/* Set register address [CS1#,CS0#,A2..A0] */
	DATL_PORT = dat;		/* Set data on the D7..D0 */
	DATL_DDR = 0xFF;		/* Set D7..D0 as output */
	CTRL_PORT &= ~IOWR;		/* IOWR# = L */
	CTRL_PORT;				/* Delay */
	CTRL_PORT |= IOWR;		/* IOWR# = H */
	DATL_PORT = 0x7F;		/* Set D7..D0 as input (pull-up wo/D7) */
	DATL_DDR = 0;
	sei();
}


#if _USE_WRITE
static
/* Write 512 byte block to ATA data register */
void write_block (
	const BYTE *buf
)
{
	BYTE c, iowr_l, iowr_h;


	CTRL_PORT = REG_DATA;	/* Select data register */
	iowr_h = REG_DATA;
	iowr_l = REG_DATA & ~IOWR;
	DATL_DDR = 0xFF; DATH_DDR = 0xFF;	/* Set D15..D0 as output */
	c = 128;
	cli();
	do {	/* Send 4 bytes/loop */
		DATL_PORT = *buf++; DATH_PORT = *buf++;	/* Set a word on the D15..D0 */
		CTRL_PORT = iowr_l; CTRL_PORT = iowr_h;	/* Make low pulse on IOWR# */
		DATL_PORT = *buf++; DATH_PORT = *buf++;	/* Set a word on the D15..D0 */
		CTRL_PORT = iowr_l; CTRL_PORT = iowr_h;	/* Make low pulse on IOWR# */
	} while (--c);
	sei();
	DATL_PORT = 0x7F; DATH_PORT = 0xFF;		/* Set D0..D15 as input (pull-up wo/D7) */
	DATL_DDR = 0; DATH_DDR = 0;
}
#endif



/*-----------------------------------------------------------------------*/
/* Wait for BSY goes 0 and the bit goes 1                                */
/*-----------------------------------------------------------------------*/

static
int wait_stat (	/* 0:Timeout or or ERR goes 1 */
	UINT ms,
	BYTE bit
)
{
	BYTE s;


	set_timer(ms);
	do {
		s = read_ata(REG_STATUS);					/* Get status */
		if (!get_timer() || (s & ERR)) return 0;	/* Abort when timeout or error occured */
	} while ((s & BSY) || (bit && !(bit & s)));		/* Wait for BSY goes 0 and the bit goes 1 */

	read_ata(REG_ALTSTAT);
	return 1;
}



/*-----------------------------------------------------------------------*/
/* Issue Read/Write command to the drive                                 */
/*-----------------------------------------------------------------------*/

static
int issue_rwcmd (
	BYTE pdrv,
	BYTE cmd,
	DWORD sector,
	UINT count
)
{

	if (!wait_stat(1000, DRDY)) return 0;
	write_ata(REG_DEV, ((BYTE)(sector >> 24) & 0x0F) | LBA | (pdrv ? DEV : 0));
	if (!wait_stat(1000, DRDY)) return 0;
	write_ata(REG_CYLH, (BYTE)(sector >> 16));
	write_ata(REG_CYLL, (BYTE)(sector >> 8));
	write_ata(REG_SECTOR, (BYTE)sector);
	write_ata(REG_COUNT, (BYTE)count);
	write_ata(REG_COMMAND, cmd);

	return 1;
}



/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv		/* Physical drive nmuber (0/1) */
)
{
	BYTE n, ex;


	if (pdrv >= N_DRIVES) return STA_NOINIT;/* Supports master/slave */
	if (Init) return Stat[pdrv];	/* Returns current status if initialization has been done */

	init_port();	/* Initialize the ATA control port and reset drives */

	write_ata(REG_DEVCTRL, SRST);	/* Set software reset */
	delay_ms(20);
	write_ata(REG_DEVCTRL, 0);		/* Release software reset */
	delay_ms(20);

	ex = 0;
	for (n = 0; n < N_DRIVES; n++) {
		wait_stat(3000, 0);		/* Select device */
		write_ata(REG_DEV, n ? DEV : 0);
		if (wait_stat(3000, DRDY)) {
			write_ata(REG_FEATURES, 0x03);	/* Set default PIO mode wo IORDY */
			write_ata(REG_COUNT, 0x01);
			write_ata(REG_COMMAND, CMD_SETFEATURES);
			if (wait_stat(1000, 0)) {
				ex |= 1 << n;
				Stat[n] = 0;
			}
		}
	}

	Init = ex;
	return Stat[pdrv];
}



/*-----------------------------------------------------------------------*/
/* Return Disk Status                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber (0/1) */
)
{
	if (pdrv >= N_DRIVES) return STA_NOINIT;	/* Supports only single drive */
	return Stat[pdrv];
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber (0/1) */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector number (LBA) */
	UINT count		/* Sector count (1..128) */
)
{
	if (pdrv >= N_DRIVES || !count || sector > 0xFFFFFFF) return RES_PARERR;
	if (Stat[pdrv] & STA_NOINIT) return RES_NOTRDY;

	/* Issue Read Setor(s) command */
	if (!issue_rwcmd(pdrv, CMD_READ, sector, count)) return RES_ERROR;

	/* Receive data blocks */
	do {
		if (!wait_stat(2000, DRQ)) return RES_ERROR; 	/* Wait for a sector prepared */
		read_block(buff);	/* Read a sector */
		buff += 512;
	} while (--count);		/* Repeat all sectors read */

	read_ata(REG_ALTSTAT);
	read_ata(REG_STATUS);

	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber (0/1) */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector number (LBA) */
	UINT count			/* Sector count (1..128) */
)
{
	if (pdrv >= N_DRIVES || !count || sector > 0xFFFFFFF) return RES_PARERR;
	if (Stat[pdrv] & STA_NOINIT) return RES_NOTRDY;

	/* Issue Write Setor(s) command */
	if (!issue_rwcmd(pdrv, CMD_WRITE, sector, count)) return RES_ERROR;

	/* Send data blocks */
	do {
		if (!wait_stat(2000, DRQ)) return RES_ERROR;	/* Wait for request to send data */
		write_block(buff);	/* Send a sector */
		buff += 512;
	} while (--count);		/* Repeat until all sector sent */

	/* Wait for end of write process */
	if (!wait_stat(1000, 0)) return RES_ERROR;
	read_ata(REG_ALTSTAT);
	read_ata(REG_STATUS);

	return RES_OK;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0/1) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive data block */
)
{
	BYTE n, w, ofs, dl, dh, *ptr = (BYTE*)buff;


	if (pdrv >= N_DRIVES) return RES_PARERR;
	if (Stat[pdrv] & STA_NOINIT) return RES_NOTRDY;

	switch (cmd) {
		case CTRL_SYNC :		/* Nothing to do */
			return RES_OK;

		case GET_SECTOR_COUNT :	/* Get number of sectors on the disk (DWORD) */
			ofs = 60; w = 2; n = 0;
			break;

		case GET_BLOCK_SIZE :	/* Get erase block size in sectors (DWORD) */
			*(DWORD*)buff = 1;
			return RES_OK;

		case ATA_GET_REV :		/* Get firmware revision (8 chars) */
			ofs = 23; w = 4; n = 4;
			break;

		case ATA_GET_MODEL :	/* Get model name (40 chars) */
			ofs = 27; w = 20; n = 20;
			break;

		case ATA_GET_SN :		/* Get serial number (20 chars) */
			ofs = 10; w = 10; n = 10;
			break;

		default:
			return RES_PARERR;
	}

	if (!wait_stat(1000, 0)) return RES_ERROR;	/* Select device */
	write_ata(REG_DEV, pdrv ? DEV : 0);
	if (!wait_stat(1000, DRDY)) return RES_ERROR;
	write_ata(REG_COMMAND, CMD_IDENTIFY);	/* Get device ID data block */
	if (!wait_stat(1000, DRQ)) return RES_ERROR;	/* Wait for data ready */
	read_block_part(ptr, ofs, w);
	while (n--) {				/* Swap byte order */
		dl = *ptr++; dh = *ptr--;
		*ptr++ = dh; *ptr++ = dl; 
	}

	read_ata(REG_ALTSTAT);
	read_ata(REG_STATUS);

	return RES_OK;
}
#endif


/*-----------------------------------------------------------------------*/
/* Device timer interrupt procedure                                      */
/*-----------------------------------------------------------------------*/
/* This function must be called in period of 10ms */

void disk_timerproc (void)
{
	UINT n;


	n = Timer;					/* 100Hz decrement timer */
	if (n) Timer = --n;
}


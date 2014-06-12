/*-----------------------------------------------------------------------*/
/* CFC and MMC combo control module                       (C)ChaN, 2013  */
/*-----------------------------------------------------------------------*/


#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include "diskio.h"

#define CFC	0	/* PD# of CompactFlash */
#define MMC	1	/* PD# of MMC/SDC */


/*---------------------------------------------------*/
/* Definitions for CF card                           */
/*---------------------------------------------------*/

/* Contorl Ports */
#define	CTRL_PORT		PORTA
#define	CTRL_DDR		DDRA
#define	SOCK_PORT		PORTC
#define	SOCK_DDR		DDRC
#define	SOCK_PIN		PINC
#define	DAT_PORT		PORTD
#define	DAT_DDR			DDRD
#define	DAT_PIN			PIND

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
#define CFPOWER			0b00000100	/* POWER# bit in the socket control port */
#define CFINS			0b00000011	/* INS# bit in the socket control port */
#define CF_EXIST		((SOCK_PIN & CFINS) == 0)	/* Both CD1, CD2 are low */

/* ATA command */
#define CMD_READ		0x20	/* READ SECTOR(S) */
#define CMD_WRITE		0x30	/* WRITE SECTOR(S) */
#define CMD_ERASE		0xC0	/* ERASE SECTOR(S) */
#define CMD_IDENTIFY	0xEC	/* DEVICE IDENTIFY */
#define CMD_SETFEATURES	0xEF	/* SET FEATURES */

/* ATA register bit definitions */
#define	LBA				0x40	/* REG_DEV */
#define	BSY				0x80	/* REG_STATUS */
#define	DRDY			0x40	/* REG_STATUS */
#define	DRQ				0x08	/* REG_STATUS */
#define	ERR				0x01	/* REG_STATUS */
#define	SRST			0x04	/* REG_DEVCTRL */
#define	NIEN			0x02	/* REG_DEVCTRL */


/*---------------------------------------------------*/
/* Definitions for MMC/SDC                           */
/*---------------------------------------------------*/

/* MMC/SD command (in SPI mode) */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */

/* Control signals and macros (Platform dependent) */
#define CS_LOW()	PORTB &= ~1		/* MMC CS = L */
#define	CS_HIGH()	PORTB |= 1		/* MMC CS = H */

#define MMC_WP	(PINB & 0x20)		/* Write protected. yes:true, no:false, default:false */
#define MMC_CD	(!(PINB & 0x10))	/* Card detected.   yes:true, no:false, default:true */

#define	FCLK_SLOW()	SPCR = 0x52		/* Set slow clock (100k-400k) */
#define	FCLK_FAST()	SPCR = 0x50		/* Set fast clock (depends on the CSD) */



/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/

static
volatile DSTATUS Stat[2] = { STA_NOINIT, STA_NOINIT };	/* Disk status {CFC, MMC}*/

static
volatile UINT Timer[2];		/* 100Hz decrement timers */

static
BYTE CardType;	/* Card type of the mounted MMC/SDC */


static
void set_timer (
	UINT ch,
	UINT ms
)
{
	ms /= 10;
	cli(); Timer[ch] = ms; sei();
}


static
UINT get_timer (
	UINT ch
)
{
	UINT n;

	cli(); n = Timer[ch]; sei();
	return n * 10;
}


static
void delay_ms (
	UINT ch,
	UINT ms
)
{
	ms /= 10;
	cli(); Timer[ch] = ms; sei();

	do {
		cli(); ms = Timer[ch]; sei();
	} while (ms);
}


/*-----------------------------------------------------------------------*/
/* CFC Power Control (Platform dependent)                                */
/*-----------------------------------------------------------------------*/

static
void CF_power_on (void)
{
	SOCK_DDR |= CFPOWER;		/* Socket power on */
	SOCK_PORT &= ~CFPOWER;
	delay_ms(0, 50);			/* 50ms */

	CTRL_PORT = IORD | IOWR;	/* Enable control signals, RESET# = L */
	CTRL_DDR = 0xFF;
	DAT_PORT = 0xFF;			/* Pull-up D7..D0 */

	CTRL_PORT |= RESET;			/* RESET# = H */
	delay_ms(0, 20);			/* 20ms */
}


static
void CF_power_off (void)
{
	DAT_PORT = 0; 					/* Set D7..D0 as hi-z */
	CTRL_DDR = 0; CTRL_PORT = 0;	/* Set bus controls as hi-z */

	SOCK_PORT |= CFPOWER;			/* Socket power off */
	SOCK_PORT |= CFINS;				/* Pull-up CD1#/CD2# */
	delay_ms(0, 100);				/* 100ms */
}



/*-----------------------------------------------------------------------*/
/* Read ATA register  (Platform dependent)                               */
/*-----------------------------------------------------------------------*/

/* Read ATA register */
static
BYTE read_ata (
	BYTE reg			/* Register to be read */
)
{
	BYTE rd;


	CTRL_PORT = reg;		/* Set address [CS1#,CS0#,A2..A0] */
	CTRL_PORT &= ~IORD;		/* IORD# = L */
	CTRL_PORT; CTRL_PORT;	/* Delay */
	rd = DAT_PIN;			/* Get data */
	CTRL_PORT |= IORD;		/* IORD# = H */
	return rd;
}


/* Read 512 bytes from ATA data register */
static
void read_block (
	BYTE *buff		/* Buffer for read data (512 bytes) */
)
{
	BYTE d, c, iord_l, iord_h;


	CTRL_PORT = REG_DATA;		/* Select data register */
	iord_h = REG_DATA;
	iord_l = REG_DATA & ~IORD;
	c = 255;
	CTRL_PORT = iord_l;		/* IORD# = L */
	CTRL_PORT; CTRL_PORT;	/* delay */
	d = DAT_PIN;			/* Get even data */
	CTRL_PORT = iord_h;		/* IORD# = H */
	do {	/* Receive 2 bytes/loop */
		CTRL_PORT = iord_l;		/* IORD# = L */
		*buff++ = d;			/* Store even data (delay) */
		d = DAT_PIN;			/* Get odd data */
		CTRL_PORT = iord_h;		/* IORD# = H */
		CTRL_PORT = iord_l;		/* IORD# = L */
		*buff++ = d;			/* Store odd data (delay) */
		d = DAT_PIN;			/* Get even data */
		CTRL_PORT = iord_h;		/* IORD# = H */
	} while (--c);	/* Repeat 255 times */
	CTRL_PORT = iord_l;		/* IORD# = L */
	*buff++ = d;			/* Store even data (delay) */
	d = DAT_PIN;			/* Get odd data */
	CTRL_PORT = iord_h;		/* IORD# = H */
	*buff++ = d;			/* Store odd data */
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
	do {
		CTRL_PORT &= ~IORD;		/* IORD# = L */
		CTRL_PORT; CTRL_PORT;	/* Delay */
		dl = DAT_PIN;			/* Read even byte */
		CTRL_PORT |= IORD;		/* IORD# = H */
		CTRL_PORT &= ~IORD;		/* IORD# = L */
		CTRL_PORT; CTRL_PORT;	/* Delay */
		dh = DAT_PIN;			/* Read odd byte */
		CTRL_PORT |= IORD;		/* IORD# = H */
		if (nw && (c >= ofs)) {	/* Pick up a part of block */
			*buf++ = dl; *buf++ = dh;
			nw--;
		}
	} while (++c);
}



/*-----------------------------------------------------------------------*/
/* Write data to ATA register (Platform dependent)                       */
/*-----------------------------------------------------------------------*/

/* Write a byte to the ATA register */
static
void write_ata (
	BYTE reg,		/* Register to be written */
	BYTE dat		/* Data to be written */
)
{
	CTRL_PORT = reg;		/* Set address [CS1#,CS0#,A2..A0] */
	DAT_DDR = 0xFF;			/* Set D7..D0 as output */
	DAT_PORT = dat;			/* Set data on the D7..D0 */
	CTRL_PORT &= ~IOWR;		/* IOWR# = L */
	CTRL_PORT;				/* Delay */
	CTRL_PORT |= IOWR;		/* IOWR# = H */
	DAT_PORT = 0xFF;		/* Set D7..D0 as input (pull-up) */
	DAT_DDR = 0;
}


/* Write 512 byte block to ATA data register */
#if _USE_WRITE
static
void write_block (
	const BYTE *buff	/* Data to write (512 bytes) */
)
{
	BYTE c, iowr_l, iowr_h;


	CTRL_PORT = REG_DATA;	/* Select data register */
	iowr_h = REG_DATA;
	iowr_l = REG_DATA & ~IOWR;
	DAT_DDR = 0xFF;			/* Set D0..D7 as output */
	c = 0;
	do {	/* Write 2 bytes/loop */
		DAT_PORT = *buff++;		/* Set even data */
		CTRL_PORT = iowr_l;		/* IOWR# = L */
		CTRL_PORT = iowr_h;		/* IOWR# = H */
		DAT_PORT = *buff++;		/* Set odd data */
		CTRL_PORT = iowr_l;		/* IOWR# = L */
		CTRL_PORT = iowr_h;		/* IOWR# = H */
	} while (--c);	/* Repeat 256 times */
	DAT_PORT = 0xFF;		/* Set D0..D7 as input (pull-up) */
	DAT_DDR = 0;
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


	set_timer(0, ms);
	do {
		s = read_ata(REG_STATUS);					/* Get status */
		if (!get_timer(0) || (s & ERR)) return 0;	/* Abort when timeout or error occured */
	} while ((s & BSY) || (bit && !(bit & s)));		/* Wait for BSY goes 0 and the bit goes 1 */

	read_ata(REG_ALTSTAT);
	return 1;
}



/*-----------------------------------------------------------------------*/
/* Issue Read/Write command to the drive                                 */
/*-----------------------------------------------------------------------*/

static
int issue_rwcmd (
	BYTE cmd,
	DWORD sector,
	BYTE count
)
{

	if (!wait_stat(1000, DRDY)) return 0;

	write_ata(REG_DEV, ((BYTE)(sector >> 24) & 0x0F) | LBA);
	write_ata(REG_CYLH, (BYTE)(sector >> 16));
	write_ata(REG_CYLL, (BYTE)(sector >> 8));
	write_ata(REG_SECTOR, (BYTE)sector);
	write_ata(REG_COUNT, count);
	write_ata(REG_COMMAND, cmd);

	return 1;
}



/*-----------------------------------------------------------------------*/
/* MMC Power Control (Platform dependent)                                */
/*-----------------------------------------------------------------------*/

static
void MM_power_on (void)
{
	{	/* Remove this block if no socket power control */
		PORTE &= ~_BV(7);		/* Socket power on (PE7=low) */
		DDRE |= _BV(7);
		delay_ms(0, 20);		/* Wait for 20ms */
	}

	PORTB |= 0b00000101;	/* Configure SCK/MOSI/CS as output */
	DDRB  |= 0b00000111;

	SPCR = 0x52;			/* Enable SPI function in mode 0 */
	SPSR = 0x01;			/* SPI 2x mode */
}


static
void MM_power_off (void)
{
	SPCR = 0;				/* Disable SPI function */

	DDRB  &= ~0b00110111;	/* Set SCK/MOSI/CS as hi-z, INS#/WP as pull-up */
	PORTB &= ~0b00000111;
	PORTB |=  0b00110000;

	{	/* Remove this block if no socket power control */
		PORTE |= _BV(7);	/* Socket power off (PE7=high) */
		delay_ms(0, 200);	/* Wait for 200ms */
	}
}



/*-----------------------------------------------------------------------*/
/* Transmit/Receive data from/to MMC via SPI  (Platform dependent)       */
/*-----------------------------------------------------------------------*/

/* Exchange a byte */
static
BYTE xchg_spi (		/* Returns received data */
	BYTE dat		/* Data to be sent */
)
{
	SPDR = dat;							/* Initiate an SPI transfer */
	loop_until_bit_is_set(SPSR, SPIF);	/* Wait for end of the transfer */
	return SPDR;						/* Return received data */
}

/* Send a data block */
static
void xmit_spi_multi (
	const BYTE *p,	/* Data block to be sent */
	UINT cnt		/* Size of data block (must be multiple of 2) */
)
{
	do {
		SPDR = *p++; loop_until_bit_is_set(SPSR,SPIF);
		SPDR = *p++; loop_until_bit_is_set(SPSR,SPIF);
	} while (cnt -= 2);
}

/* Receive a data block */
static
void rcvr_spi_multi (
	BYTE *p,	/* Data buffer */
	UINT cnt	/* Size of data block (must be multiple of 2) */
)
{
	do {
		SPDR = 0xFF; loop_until_bit_is_set(SPSR,SPIF); *p++ = SPDR;
		SPDR = 0xFF; loop_until_bit_is_set(SPSR,SPIF); *p++ = SPDR;
	} while (cnt -= 2);
}



/*-----------------------------------------------------------------------*/
/* Wait for MMC ready                                                    */
/*-----------------------------------------------------------------------*/

static
int wait_ready (void)
{
	BYTE d;

	set_timer(1, 500);	/* Wait for ready in timeout of 500ms */
	do
		d = xchg_spi(0xFF);
	while (d != 0xFF && get_timer(1));

	return d == 0xFF ? 1 : 0;
}



/*-----------------------------------------------------------------------*/
/* Deselect the MMC and release SPI bus                                  */
/*-----------------------------------------------------------------------*/

static
void deselect (void)
{
	CS_HIGH();
	xchg_spi(0xFF);	/* Dummy clock (force DO hi-z for multiple slave SPI) */
}



/*-----------------------------------------------------------------------*/
/* Select the MMC and wait ready                                         */
/*-----------------------------------------------------------------------*/

static
int select (void)	/* 1:Successful, 0:Timeout */
{
	CS_LOW();
	xchg_spi(0xFF);	/* Dummy clock (force DO enabled) */

	if (wait_ready()) return 1;	/* OK */
	deselect();
	return 0;	/* Timeout */
}



/*-----------------------------------------------------------------------*/
/* Receive a data packet from MMC                                        */
/*-----------------------------------------------------------------------*/

static
int rcvr_datablock (
	BYTE *buff,			/* Data buffer to store received data */
	UINT btr			/* Byte count (must be multiple of 4) */
)
{
	BYTE token;


	set_timer(0, 200);
	do {							/* Wait for data packet in timeout of 200ms */
		token = xchg_spi(0xFF);
	} while ((token == 0xFF) && get_timer(0));
	if (token != 0xFE) return 0;	/* If not valid data token, retutn with error */

	rcvr_spi_multi(buff, btr);		/* Receive the data block into buffer */
	xchg_spi(0xFF);					/* Discard CRC */
	xchg_spi(0xFF);

	return 1;						/* Return with success */
}



/*-----------------------------------------------------------------------*/
/* Send a data packet to MMC                                             */
/*-----------------------------------------------------------------------*/

#if	_USE_WRITE
static
int xmit_datablock (
	const BYTE *buff,	/* 512 byte data block to be transmitted */
	BYTE token			/* Data/Stop token */
)
{
	BYTE resp;


	if (!wait_ready()) return 0;

	xchg_spi(token);				/* Transmit data token */
	if (token != 0xFD) {	/* Is data token */
		xmit_spi_multi(buff, 512);	/* Transmit the data block to the MMC */
		xchg_spi(0xFF);				/* CRC (Dummy) */
		xchg_spi(0xFF);
		resp = xchg_spi(0xFF);		/* Reveive data response */
		if ((resp & 0x1F) != 0x05)	/* If not accepted, return with error */
			return 0;
	}

	return 1;
}
#endif



/*-----------------------------------------------------------------------*/
/* Send a command packet to MMC                                          */
/*-----------------------------------------------------------------------*/

static
BYTE send_cmd (	/* Returns R1 resp (bit7==1:Send failed) */
	BYTE cmd,	/* Command index */
	DWORD arg	/* Argument */
)
{
	BYTE n, res;


	if (cmd & 0x80) {	/* ACMD<n> is the command sequense of CMD55-CMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) return res;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		deselect();
		if (!select()) return 0xFF;
	}

	/* Send command packet */
	xchg_spi(0x40 | cmd);			/* Start + Command index */
	xchg_spi((BYTE)(arg >> 24));	/* Argument[31..24] */
	xchg_spi((BYTE)(arg >> 16));	/* Argument[23..16] */
	xchg_spi((BYTE)(arg >> 8));		/* Argument[15..8] */
	xchg_spi((BYTE)arg);			/* Argument[7..0] */
	n = 0x01;						/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;		/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;		/* Valid CRC for CMD8(0x1AA) */
	xchg_spi(n);

	/* Receive command response */
	if (cmd == CMD12) xchg_spi(0xFF);	/* Skip a stuff byte when stop reading */
	n = 10;							/* Wait for a valid response in timeout of 10 attempts */
	do
		res = xchg_spi(0xFF);
	while ((res & 0x80) && --n);

	return res;			/* Return with the response value */
}



/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

static
DSTATUS CF_disk_initialize (void)
{
	CF_power_off();							/* Turn off the socket power to reset the card */
	Stat[CFC] |= STA_NOINIT;
	if (Stat[CFC] & STA_NODISK) return Stat[CFC];	/* Exit if socket is empty */

	CF_power_on();							/* Turn on the socket power */

	write_ata(REG_DEVCTRL, SRST);			/* Set software reset */
	delay_ms(0, 20);
	write_ata(REG_DEVCTRL, 0);				/* Release software reset */
	delay_ms(0, 20);

	if (!wait_stat(3000, 0)) return Stat[CFC];	/* Select device 0 */
	write_ata(REG_DEV, 0);
	if (!wait_stat(3000, DRDY)) return Stat[CFC];

	write_ata(REG_FEATURES, 0x03);			/* Set default PIO mode wo IORDY */
	write_ata(REG_COUNT, 0x01);
	write_ata(REG_COMMAND, CMD_SETFEATURES);
	if (!wait_stat(3000, DRDY)) return Stat[CFC];

	write_ata(REG_FEATURES, 0x01);			/* Select 8-bit PIO transfer mode */
	write_ata(REG_COMMAND, CMD_SETFEATURES);
	if (!wait_stat(1000, DRDY)) return Stat[CFC];

	Stat[CFC] &= ~STA_NOINIT;				/* Initialization succeeded */

	return Stat[CFC];
}


static
DSTATUS MM_disk_initialize (void)
{
	BYTE n, ty, cmd, ocr[4];


	MM_power_off();							/* Turn off the socket power to reset the card */
	if (Stat[MMC] & STA_NODISK) return Stat[1];	/* No card in the socket? */
	MM_power_on();							/* Turn on the socket power */
	FCLK_SLOW();
	for (n = 10; n; n--) xchg_spi(0xFF);	/* 80 dummy clocks */

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) {			/* Enter Idle state */
		set_timer(0, 1000);					/* Initialization timeout of 1000 msec */
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDC ver 2.00 */
			for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {	/* The card can work at vdd range of 2.7-3.6V */
				while (get_timer(0) && send_cmd(ACMD41, 1UL << 30));	/* ACMD41 with HCS bit */
				if (get_timer(0) && send_cmd(CMD58, 0) == 0) {	/* Check CCS bit */
					for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
					ty = (ocr[0] & 0x40) ? CT_SD2|CT_BLOCK : CT_SD2;	/* SDv2 */
				}
			}
		} else {							/* SDv1 or MMCv3 */
			if (send_cmd(ACMD41, 0) <= 1) {
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 */
			}
			while (get_timer(0) && send_cmd(cmd, 0));	/* Wait for leaving idle state */
			if (!get_timer(0) || send_cmd(CMD16, 512) != 0)	/* Select R/W block length */
				ty = 0;
		}
	}
	CardType = ty;
	deselect();

	if (ty) {			/* Initialization succeded */
		Stat[MMC] &= ~STA_NOINIT;	/* Clear STA_NOINIT */
		FCLK_FAST();
	} else {			/* Initialization failed */
		MM_power_off();
		Stat[MMC] |= STA_NOINIT;/* Force uninitialized */
	}

	return Stat[MMC];
}



DSTATUS disk_initialize (
	BYTE pdrv		/* Physical drive nmuber (0) */
)
{
	switch (pdrv) {
	case CFC :
		return CF_disk_initialize();
	case MMC :
		return MM_disk_initialize();
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Return Disk Status                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber */
)
{
	switch (pdrv) {
	case CFC :
		return Stat[CFC];
	case MMC :
		return Stat[MMC];
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

static
DRESULT CF_disk_read (
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector number (LBA) */
	UINT count		/* Sector count (1..128) */
)
{
	if (!count) return RES_PARERR;
	if (Stat[CFC] & STA_NOINIT) return RES_NOTRDY;

	/* Issue Read Setor(s) command */
	if (!issue_rwcmd(CMD_READ, sector, count)) return RES_ERROR;

	/* Receive data blocks */
	do {
		if (!wait_stat(2000, DRQ)) return RES_ERROR;
		read_block(buff);
		buff += 512;
	} while (--count);

	read_ata(REG_ALTSTAT);
	read_ata(REG_STATUS);

	return RES_OK;
}



static
DRESULT MM_disk_read (
	BYTE *buff,			/* Pointer to the data buffer to store read data */
	DWORD sector,		/* Start sector number (LBA) */
	UINT count			/* Sector count (1..128) */
)
{
	BYTE cmd;


	if (!count) return RES_PARERR;
	if (Stat[MMC] & STA_NOINIT) return RES_NOTRDY;

	if (!(CardType & CT_BLOCK)) sector *= 512;	/* Convert LBA to byte address if needed */

	cmd = count > 1 ? CMD18 : CMD17;			/*  READ_MULTIPLE_BLOCK : READ_SINGLE_BLOCK */
	if (send_cmd(cmd, sector) == 0) {
		do {
			if (!rcvr_datablock(buff, 512)) break;
			buff += 512;
		} while (--count);
		if (cmd == CMD18) send_cmd(CMD12, 0);	/* STOP_TRANSMISSION */
	}
	deselect();

	return count ? RES_ERROR : RES_OK;
}



DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber (0) */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector number (LBA) */
	UINT count		/* Sector count (1..128) */
)
{
	switch (pdrv) {
	case CFC :
		return CF_disk_read(buff, sector, count);
	case MMC :
		return MM_disk_read(buff, sector, count);
	}
	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
static
DRESULT CF_disk_write (
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector number (LBA) */
	UINT count			/* Sector count (1..128) */
)
{
	if (!count) return RES_PARERR;
	if (Stat[0] & STA_NOINIT) return RES_NOTRDY;

	/* Issue Write Setor(s) command */
	if (!issue_rwcmd(CMD_WRITE, sector, count)) return RES_ERROR;

	/* Send data blocks */
	do {
		if (!wait_stat(2000, DRQ)) return RES_ERROR;
		write_block(buff);
		buff += 512;
	} while (--count);

	if (!wait_stat(1000, 0)) return RES_ERROR;
	read_ata(REG_ALTSTAT);
	read_ata(REG_STATUS);

	return RES_OK;
}


static
DRESULT MM_disk_write (
	const BYTE *buff,	/* Pointer to the data to be written */
	DWORD sector,		/* Start sector number (LBA) */
	UINT count			/* Sector count (1..128) */
)
{
	if (!count) return RES_PARERR;
	if (Stat[1] & STA_NOINIT) return RES_NOTRDY;
	if (Stat[1] & STA_PROTECT) return RES_WRPRT;

	if (!(CardType & CT_BLOCK)) sector *= 512;	/* Convert LBA to byte address if needed */

	if (count == 1) {	/* Single block write */
		if ((send_cmd(CMD24, sector) == 0)	/* WRITE_BLOCK */
			&& xmit_datablock(buff, 0xFE))
			count = 0;
	}
	else {				/* Multiple block write */
		if (CardType & CT_SDC) {
			send_cmd(CMD55, 0); send_cmd(CMD23, count);	/* ACMD23 */
		}
		if (send_cmd(CMD25, sector) == 0) {	/* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD))	/* STOP_TRAN token */
				count = 1;
		}
	}
	deselect();

	return count ? RES_ERROR : RES_OK;
}


DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber (0) */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector number (LBA) */
	UINT count			/* Sector count (1..128) */
)
{
	switch (pdrv) {
	case CFC :
		return CF_disk_write(buff, sector, count);
	case MMC :
		return MM_disk_write(buff, sector, count);
	}
	return RES_PARERR;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
static
DRESULT CF_disk_ioctl (
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive data block */
)
{
	BYTE n, w, ofs, dl, dh, *ptr = (BYTE*)buff;


	if (Stat[CFC] & STA_NOINIT) return RES_NOTRDY;

	switch (cmd) {
	case GET_BLOCK_SIZE :	/* Get erase block size in sectors (DWORD) */
		*(DWORD*)buff = 128;
		return RES_OK;
	case CTRL_SYNC :		/* Nothing to do */
		return RES_OK;
	case CTRL_POWER_OFF :	/* Power off */
		CF_power_off();
		Stat[CFC] |= STA_NOINIT;
		break;
	}

	switch (cmd) {
	case GET_SECTOR_COUNT :
		ofs = 60; w = 2; n = 0;
		break;
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

	if (!wait_stat(1000, DRDY)) return RES_ERROR;
	write_ata(REG_COMMAND, CMD_IDENTIFY);	/* Get device ID data block */
	if (!wait_stat(1000, DRQ)) return RES_ERROR;	/* Wait for data ready */
	read_block_part(ptr, ofs, w);					/* Get a part of data block */
	while (n--) {				/* Swap byte order */
		dl = *ptr++; dh = *ptr--;
		*ptr++ = dh; *ptr++ = dl; 
	}
	read_ata(REG_ALTSTAT);
	read_ata(REG_STATUS);

	return RES_OK;
}


static
DRESULT MM_disk_ioctl (
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive data block */
)
{
	DRESULT res;
	BYTE n, csd[16], *ptr = buff;
	DWORD csize;


	if (Stat[MMC] & STA_NOINIT) return RES_NOTRDY;

	res = RES_ERROR;
	switch (cmd) {
	case CTRL_SYNC :	/* Make sure that pending write process has been finished */
		if (select()) res = RES_OK;
		break;

	case GET_SECTOR_COUNT :	/* Get number of sectors on the disk (DWORD) */
		if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
			if ((csd[0] >> 6) == 1) {	/* SDC ver 2.00 */
				csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
				*(DWORD*)buff = csize << 10;
			} else {					/* MMC or SDC ver 1.XX */
				n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
				csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
				*(DWORD*)buff = csize << (n - 9);
			}
			res = RES_OK;
		}
		break;

	case GET_BLOCK_SIZE :	/* Get erase block size in unit of sectors (DWORD) */
		if (CardType & CT_SD2) {	/* SDC ver 2.00 */
			if (send_cmd(ACMD13, 0) == 0) {	/* Read SD status */
				xchg_spi(0xFF);
				if (rcvr_datablock(csd, 16)) {				/* Read partial block */
					for (n = 64 - 16; n; n--) xchg_spi(0xFF);	/* Purge trailing data */
					*(DWORD*)buff = 16UL << (csd[10] >> 4);
					res = RES_OK;
				}
			}
		} else {					/* SDC ver 1.XX or MMC */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {	/* Read CSD */
				if (CardType & CT_SD1) {	/* SDC ver 1.XX */
					*(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
				} else {					/* MMC */
					*(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
				}
				res = RES_OK;
			}
		}
		break;

	case MMC_GET_CSD :		/* Receive CSD as a data block (16 bytes) */
		if (send_cmd(CMD9, 0) == 0		/* READ_CSD */
			&& rcvr_datablock(ptr, 16))
			res = RES_OK;
		break;

	case MMC_GET_CID :		/* Receive CID as a data block (16 bytes) */
		if (send_cmd(CMD10, 0) == 0		/* READ_CID */
			&& rcvr_datablock(ptr, 16))
			res = RES_OK;
		break;

	case MMC_GET_OCR :		/* Receive OCR as an R3 resp (4 bytes) */
		if (send_cmd(CMD58, 0) == 0) {	/* READ_OCR */
			for (n = 0; n < 4; n++)
				*ptr++ = xchg_spi(0xFF);
			res = RES_OK;
		}
		break;

	case MMC_GET_SDSTAT :	/* Receive SD statsu as a data block (64 bytes) */
		if (send_cmd(ACMD13, 0) == 0) {		/* SD_STATUS */
			xchg_spi(0xFF);
			if (rcvr_datablock(ptr, 64))
				res = RES_OK;
		}
		break;

	case CTRL_POWER_OFF :	/* Power off */
		MM_power_off();
		Stat[MMC] |= STA_NOINIT;
		break;

	default:
		res = RES_PARERR;
	}

	deselect();

	return res;
}


DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive data block */
)
{
	switch (pdrv) {
	case CFC :
		return CF_disk_ioctl(cmd, buff);
	case MMC :
		return MM_disk_ioctl(cmd, buff);
	}
	return RES_PARERR;
}
#endif


/*-----------------------------------------------------------------------*/
/* Device timer interrupt procedure                                      */
/*-----------------------------------------------------------------------*/
/* This must be called in period of 10ms                                 */

void disk_timerproc (void)
{
	BYTE n;
	UINT d;
	DSTATUS s;


	for (n = 0; n < 2; n++) {		/* 100Hz decrement timers */
		d = Timer[n];
		if (d) Timer[n] = --d;
	}

	/* CF control */
	if (!CF_EXIST) {			/* Socket empty */
		Stat[CFC] |= STA_NODISK | STA_NOINIT;
	} else {					/* Card inserted */
		Stat[CFC] &= ~STA_NODISK;
	}

	/* MMC control */
	s = Stat[MMC];
	if (MMC_WP)		/* MMC write protected */
		s |= STA_PROTECT;
	else			/* MMC not write enabled */
		s &= ~STA_PROTECT;
	if (MMC_CD)		/* MMC inserted */
		s &= ~STA_NODISK;
	else			/* MMC socket empty */
		s |= (STA_NODISK | STA_NOINIT);
	Stat[MMC] = s;

}


/*------------------------------------------------------------------------/
/  Display control module for SSD1339/SSD1355
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2011, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/

#include <string.h>
#include "LPC2300.h"
#include "interrupt.h"
#include "xprintf.h"
#include "disp.h"
#include "ff.h"
#include "sound.h"
#include "uart23xx.h"
#include "tjpgd.h"

#define USE_DBCS	1	/* 0:ANK only, 1:Enable kanji chars */

#define	DISP_TYPE	(FIO4PIN1 & _BV(7))	/* P4.15(Module type): H=SSD1339, L=SSD1355 */

#define	CMD_WRB(d)	{ FIO4PIN0 = (d); FIO4CLR1 = _BV(3); FIO4CLR1=_BV(1); FIO4SET1=_BV(1); }	/* Write a command to the OLED */
#define	DATA_WRB(d)	{ FIO4PIN0 = (d); FIO4SET1 = _BV(3); FIO4CLR1=_BV(1); FIO4SET1=_BV(1); }	/* Write a byte to the OLED */
#define	DATA_WPX(d)	{ FIO4SET1 = _BV(3); FIO4PIN0 = ((d)>>8); FIO4CLR1=_BV(1); FIO4SET1=_BV(1); FIO4PIN0 = (d); FIO4CLR1=_BV(1); FIO4SET1=_BV(1); }	/* Write a pixel to the OLED */



static int MaskT, MaskL, MaskR, MaskB;	/* Drawing mask */
static int LocX, LocY;			/* Current dot position */
static uint32_t ChrColor;		/* Current character color ((bg << 16) + fg) */
static const uint8_t *FontS;	/* Current font (ANK) */
#if USE_DBCS
static const uint8_t *FontD;	/* Current font (Kanji) */
static uint8_t Sjis;			/* Sjis leading byte */
#endif

volatile long TmrFrm;			/* Timer (increased 1000 every 1ms) */

/* Import FONTX2 files as byte array */
IMPORT_BIN(".rodata", "FONT24S.FNT", FontH24);			/* const uint8_t FontH24[] */
IMPORT_BIN(".rodata", "mplfont/MPLHN10X.FNT", FontH10);	/* const uint8_t FontH10[] */
IMPORT_BIN(".rodata", "mplfont/MPLZN10X.FNT", FontZ10);	/* const uint8_t FontZ10[] */



/*-----------------------------------------------------*/
/* Set rectangular area to be transferred              */

static
void disp_setrect (
	int left,		/* Left end (0..127) */
	int right,		/* Right end (0..127, >= left) */
	int top,		/* Top end (0..127) */
	int bottom		/* Bottom end (0..127, >= top) */
)
{
	CMD_WRB(DISP_TYPE ? 0x15 : 0x2A);	/* Set H range */
	DATA_WRB(left); DATA_WRB(right); 

	CMD_WRB(DISP_TYPE ? 0x75 : 0x2B);	/* Set V range */
	DATA_WRB(top); DATA_WRB(bottom); 

	CMD_WRB(DISP_TYPE ? 0x5C : 0x2C);	/* Ready to receive pixel data */
}



/*-----------------------------------------------------*/
/* Initialize display module (SSD1339 or SSD1355)      */

void disp_init (void)
{
	static const BYTE ssd1339[] = {
		2, 0xA0, 0x74,		// Set Re-map / Color Depth (64K color, COM split, COM remap, 8-bit, Color remap, Non column address remap, Hotizontal increment)
		2, 0xA1, 0x00,		// Set display start line (0)
		2, 0xA2, 0x80,		// Set display offset (128)
		1, 0xA6,			// Normal display
		2, 0xAD, 0x8E,		// Set Master Configuration (DC-DC off & external VcomH voltage & external pre-charge voltage)
		2, 0xB0, 0x05,		// Power saving mode
		2, 0xB1, 0x11, 		// Set pre & dis_charge (pre=1h dis=1h)
		2, 0xB3, 0x61,		// clock & frequency (clock=Divser+2 frequency=6)
		4, 0xBB, 0x1C, 0x1C, 0x1C,	// Set pre-charge voltage of color A B C
		2, 0xBE, 0x1F,		// Set VcomH
	//	4, 0xC1, 0xAA, 0xB4, 0xC8,	// Set contrast current for A B C
	//	2, 0xC7, 0x0F,		// Set master contrast
		2, 0xCA, 0x7F,		// Duty
		0,
		0xAF				// Display on
	};
	 static const BYTE ssd1355[] = {
		2, 0xFD, 0xB3,		// Unlock all commands
		1, 0x11,			// Sleep out
		2, 0x3A, 0x05,		// Interface Pixel Format (64K color)
		2, 0xD2, 0xA1,		// Set display clock divider (Fosc=10, Dclkdiv=2)
		2, 0xC8, 0x00,		// Set display offset
		3, 0x36, 0x88, 0x01,// MADCTRL
		2, 0xD3, 0x04,		// Set VcomH (0.8 Vcc)
		2, 0xCA, 0x7F,		// Set MUX ratio (128 mux)
		2, 0xCD, 0x74,		// Set precharge length (9 DCLK + 7 DCLK)
		2, 0xCE, 0x07,		// Set second precharge period (7 DCLK)
		2, 0xCF, 0x02,		// Set second precharge speed (Normal)
		2, 0xBA, 0x40,		// Set contrast current for A (R = 0x40)
		2, 0xBB, 0x48,		// Set contrast current for B (G = 0x50)
		2, 0xBC, 0x46,		// Set contrast current for C (B = 0x4E)
		2, 0xBD, 0x08,		// Set first precharge voltage
		97, 0xBE, 0, 0, 0, 1, 3, 4, 6, 8, 10, 13, 15, 18, 21, 25, 28, 32, 36, 40, 45, 50, 54, 59, 65, 70, 76, 82, 88, 94, 100, 107, 113, 120, 0, 0, 0, 1, 3, 4, 6, 8, 10, 13, 15, 18, 21, 25, 28, 32, 36, 40, 45, 50, 54, 59, 65, 70, 76, 82, 88, 94, 100, 107, 113, 120, 0, 0, 0, 1, 3, 4, 6, 8, 10, 13, 15, 18, 21, 25, 28, 32, 36, 40, 45, 50, 54, 59, 65, 70, 76, 82, 88, 94, 100, 107, 113, 120, // Set gamma table
		0,
		0x13,			// Display on
	};
	const BYTE *p;
	BYTE cmd;
	int n, i;


	/* Initialize display module control port */
	FIO4PINL = 0x1FFF;		/* P4L: TYPE|-|-|RES#|D/C#|CS#|WR#|RD#|D7|D6|D5|D4|D3|D2|D1|D0 */
	FIO4DIRL = 0x1FFF;

	/* Reset display module */
	FIO4SET1 = 0x03;	// RD=H, WR=H
	FIO4CLR1 = 0x14;	// RES=L, CS=L
	for (TmrFrm = 0; TmrFrm < 20000; ) ;	/* 20ms */
	FIO4SET1 = 0x18;	// RES=H, DC=H
	for (TmrFrm = 0; TmrFrm < 20000; ) ;	/* 20ms */

	/* Send initialization data */
	p = DISP_TYPE ? ssd1339 : ssd1355;
	while ((n = *p++) != 0) {
		cmd = *p++; n--;
		CMD_WRB(cmd);
		for (i = 0; i < n; i++) DATA_WRB(*p++);
	}

	/* Clear screen and Display ON */
	disp_mask(0, DISP_XS - 1, 0, DISP_YS - 1);
	disp_fill(0, DISP_XS - 1, 0, DISP_YS - 1, 0x0000);
	CMD_WRB(*p);

	/* Register text fonts */
	disp_font_face(FontH10);	/* ANK font */
	disp_font_face(FontZ10);	/* Kanji font */
}



/*-----------------------------------------------------*/
/* Set active drawing area                             */
/*-----------------------------------------------------*/
/* The mask feature affects only disp_fill, disp_box,  */
/* disp_pset, disp_lineto and disp_blt function        */

void disp_mask (
	int left,		/* Left end of active window (0..DISP_XS-1) */
	int right,		/* Right end of active window (0..DISP_XS-1, >=left) */
	int top,		/* Top end of active window (0..DISP_YS-1) */
	int bottom		/* Bottom end of active window (0..DISP_YS-1, >=top) */
)
{
	if (left >= 0 && right < DISP_XS && left <= right && top >= 0 && bottom < DISP_XS && top <= bottom) {
		MaskL = left;
		MaskR = right;
		MaskT = top;
		MaskB = bottom;
	}
}



/*-----------------------------------------------------*/
/* Draw a solid rectangular                            */

void disp_fill (
	int left,		/* Left end (-32768..32767) */
	int right,		/* Right end (-32768..32767, >=left) */
	int top,		/* Top end (-32768..32767) */
	int bottom,		/* Bottom end (-32768..32767, >=top) */
	uint16_t color	/* Box color */
)
{
	uint32_t n;


	if (left > right || top > bottom) return; 	/* Check varidity */
	if (left > MaskR || right < MaskL  || top > MaskB || bottom < MaskT) return;	/* Check if in active area */

	if (top < MaskT) top = MaskT;		/* Clip top of rectangular if it is out of active area */
	if (bottom > MaskB) bottom = MaskB;	/* Clip bottom of rectangular if it is out of active area */
	if (left < MaskL) left = MaskL;		/* Clip left of rectangular if it is out of active area */
	if (right > MaskR) right = MaskR;	/* Clip right of rectangular if it is out of active area */

	disp_setrect(left, right, top, bottom);
	n = (uint32_t)(right - left + 1) * (uint32_t)(bottom - top + 1);
	do { DATA_WPX(color); } while (--n);
}




/*-----------------------------------------------------*/
/* Draw a hollow rectangular                           */

void disp_box (
	int left,		/* Left end (-32768..32767) */
	int right,		/* Right end (-32768..32767, >=left) */
	int top,		/* Top end (-32768..32767) */
	int bottom,		/* Bottom end (-32768..32767, >=right) */
	uint16_t color	/* Box color */
)
{
	disp_fill(left, left, top, bottom, color);
	disp_fill(right, right, top, bottom, color);
	disp_fill(left, right, top, top, color);
	disp_fill(left, right, bottom, bottom, color);
}



/*-----------------------------------------------------*/
/* Draw a dot                                          */

void disp_pset (
	int x,		/* X position (-32768..32767) */
	int y,		/* Y position (-32768..32767) */
	uint16_t color	/* Pixel color */
)
{
	if (x >= MaskL && x <= MaskR && y >= MaskT && y <= MaskB) {
		CMD_WRB(DISP_TYPE ? 0x15 : 0x2A);	/* Set H position */
		DATA_WRB(x); DATA_WRB(x);
		CMD_WRB(DISP_TYPE ? 0x75 : 0x2B);	/* Set V position */
		DATA_WRB(y); DATA_WRB(y);
		CMD_WRB(DISP_TYPE ? 0x5C : 0x2C);	/* Write a pixel data */
		DATA_WPX(color);
	}
}



/*-----------------------------------------------------*/
/* Set current dot position for disp_lineto            */

void disp_moveto (
	int x,		/* X position (-32768..32767) */
	int y		/* Y position (-32768..32767) */
)
{
	LocX = x;
	LocY = y;
}



/*-----------------------------------------------------*/
/* Draw a line from current position                   */

void disp_lineto (
	int x,		/* X position for the line to (-32768..32767) */
	int y,		/* Y position for the line to (-32768..32767) */
	uint16_t col	/* Line color */
)
{
	int32_t xr, yr, xd, yd;
	int ctr;


	xd = x - LocX; xr = LocX << 16; LocX = x;
	yd = y - LocY; yr = LocY << 16; LocY = y;

	if ((xd < 0 ? 0 - xd : xd) >= (yd < 0 ? 0 - yd : yd)) {
		ctr = (xd < 0 ? 0 - xd : xd) + 1;
		yd = (yd << 16) / (xd < 0 ? 0 - xd : xd);
		xd = (xd < 0 ? -1 : 1) << 16;
	} else {
		ctr = (yd < 0 ? 0 - yd : yd) + 1;
		xd = (xd << 16) / (yd < 0 ? 0 - yd : yd);
		yd = (yd < 0 ? -1 : 1) << 16;
	}
	xr += 1 << 15;
	yr += 1 << 15;
	do {
		disp_pset(xr >> 16, yr >> 16, col);
		xr += xd; yr += yd;
	} while (--ctr);

}



/*-----------------------------------------------------*/
/* Copy image data to the display                      */

void disp_blt (
	int left,		/* Left end (-32768..32767) */
	int right,		/* Right end (-32768..32767, >=left) */
	int top,		/* Top end (-32768..32767) */
	int bottom,		/* Bottom end (-32768..32767, >=right) */
	const uint16_t *pat	/* Pattern data */
)
{
	int yc, xc, xl, xs;
	uint16_t pd;


	if (left > right || top > bottom) return; 	/* Check varidity */
	if (left > MaskR || right < MaskL  || top > MaskB || bottom < MaskT) return;	/* Check if in active area */

	yc = bottom - top + 1;			/* Vertical size */
	xc = right - left + 1; xs = 0;	/* Horizontal size and skip */

	if (top < MaskT) {		/* Clip top of source image if it is out of active area */
		pat += xc * (MaskT - top);
		yc -= MaskT - top;
		top = MaskT;
	}
	if (bottom > MaskB) {	/* Clip bottom of source image if it is out of active area */
		yc -= bottom - MaskB;
		bottom = MaskB;
	}
	if (left < MaskL) {		/* Clip left of source image if it is out of active area */
		pat += MaskL - left;
		xc -= MaskL - left;
		xs += MaskL - left;
		left = MaskL;
	}
	if (right > MaskR) {	/* Clip right of source image it is out of active area */
		xc -= right - MaskR;
		xs += right - MaskR;
		right = MaskR;
	}

	disp_setrect(left, right, top, bottom);	/* Set rectangular area to fill */
	do {	/* Send image data */
		xl = xc / 2;
		while (xl--) {
			pd = *pat++; DATA_WPX(pd);
			pd = *pat++; DATA_WPX(pd);
		}
		if (xc & 1) {
			pd = *pat++; DATA_WPX(pd);
		}
		pat += xs;
	} while (--yc);
}



/*-----------------------------------------------------*/
/* Set current character position for disp_putc        */

void disp_locate (
	int col,	/* Column position */
	int row		/* Row position */
)
{
	if (FontS) {	/* Pixel position is calcurated with size of single byte font */
		LocX = col * FontS[14];
		LocY = row * FontS[15];
#if USE_DBCS
		Sjis = 0;
#endif
	}
}



/*-----------------------------------------------------*/
/* Register text font                                  */

void disp_font_face (
	const uint8_t *font	/* Pointer to the font structure in FONTX2 format */
)
{
	if (!memcmp(font, "FONTX2", 6)) {
#if USE_DBCS
		if (font[16] != 0)
			FontD = font;
		else
#endif
			FontS = font;
	}
}



/*-----------------------------------------------------*/
/* Set current text color                              */

void disp_font_color (
	uint32_t color	/* (bg << 16) + fg */
)
{
	ChrColor = color;
}



/*-----------------------------------------------------*/
/* Put a text character                                */

void disp_putc (
	uint8_t chr		/* Character to be output (kanji chars are given in two disp_putc sequence) */
)
{
	const uint8_t *fnt;;
	uint8_t b, d;
	uint16_t dchr;
	uint32_t col;
	int h, wc, w, wb, i, fofs;


	if ((fnt = FontS) == 0) return;	/* Exit if no font registerd */

	if (chr < 0x20) {	/* Processes the control character */
#if USE_DBCS
		Sjis = 0;
#endif
		switch (chr) {
		case '\n':	/* LF */
			LocY += fnt[15];
			/* follow next case */
		case '\r':	/* CR */
			LocX = 0;
			return;
		case '\b':	/* BS */
			LocX -= fnt[14];
			if (LocX < 0) LocX = 0;
			return;
		case '\f':	/* FF */
			disp_fill(0, DISP_XS - 1, 0, DISP_YS - 1, 0);
			LocX = LocY = 0;
			return;
		}
	}

	/* Exit if current position is out of screen */
	if ((unsigned int)LocX >= DISP_XS || (unsigned int)LocY >= DISP_YS) return;

#if USE_DBCS
	if (Sjis) {	/* This is sjis trailing byte */
		uint16_t bchr, rs, re;
		int ri;

		dchr = Sjis * 256 + chr; 
		Sjis = 0;
		fnt = FontD;	/* Switch to double byte font */
		i = fnt[17];	/* Number of code blocks */
		ri = 18;		/* Start of code block table */
		bchr = 0;		/* Number of chars in previous blocks */
		while (i) {		/* Find the code in the code blocks */
			rs = fnt[ri + 0] + fnt[ri + 1] * 256;	/* Start of a block */
			re = fnt[ri + 2] + fnt[ri + 3] * 256;	/* End of a block */
			if (dchr >= rs && dchr <= re) break;	/* Is the code in the block? */
			bchr += re - rs + 1; ri += 4; i--;		/* Next block */
		}
		if (!i) {	/* Code not found */
			LocX += fnt[14];		/* Put a transparent character */
			return;
		}
		dchr = dchr - rs + bchr;	/* Character offset in the font area */
		fofs = 18 + fnt[17] * 4;	/* Font area start address */
	} else {
		/* Check if this is sjis leading byte */
		if (FontD && (((uint8_t)(chr - 0x81) <= 0x1E) || ((uint8_t)(chr - 0xE0) <= 0x1C))) {
			Sjis = chr;	/* Next is sjis trailing byte */
			return;
		}
#endif
		dchr = chr;
		fofs = 17;		/* Font area start address */
#if USE_DBCS
	}
#endif

	h = fnt[15]; w = fnt[14]; wb = (w + 7) / 8;	/* Font size: height, dot width and byte width */
	fnt += fofs + dchr * wb * h;				/* Goto start of the bitmap */

	if (LocX + w > DISP_XS) w = DISP_XS - LocX;	/* Clip right of font face at right edge */
	if (LocY + h > DISP_YS) h = DISP_YS - LocY;	/* Clip bottom of font face at bottom edge */

	disp_setrect(LocX, LocX + w - 1, LocY, LocY + h - 1);
	d = 0;
	do {
		wc = w; b = i = 0;
		do {
			if (!b) {		/* Get next 8 bits */
				b = 0x80;
				d = fnt[i++];
			}
			col = ChrColor;
			if (!(b & d)) col >>= 16;	/* Select color, BG or FG */
			b >>= 1;		/* Next bit */
			DATA_WPX(col);	/* Put the color */
		} while (--wc);
		fnt += wb;		/* Next raster */
	} while (--h);

	LocX += w;	/* Update current position */
}



#if _USE_FILE_LOADER
/*-----------------------------------------------------*/
/* BMP/IMG/TXT file loaders                            */


/* BMP file viewer scroll step */
#define MOVE_X	(DISP_XS / 4)	/* X scroll step */
#define MOVE_Y	(DISP_YS / 4)	/* Y scroll step */

/* IMG file header */
#define imSign			0	/* Signature "IM" or "im" */
#define	imWidth			2	/* Frame width (pix) */
#define	imHeight		4	/* Frame height (pix) */
#define	imBpp			6	/* Number of bits per pixel */
#define	imDataOfs		8	/* Data start offset */
#define	imFrames		12	/* Nuber of frames */
#define	imFrmPeriod		16	/* Frame period in unit of us */
#define	imFrmSize		20	/* Frame (picture+wav) size in unit of byte */
#define	imWavSamples	24	/* Number of audio samples */
#define	imWavFormat		28	/* Audio format b0:mono(0),stereo(1), b1:8bit(0),16bit(1) */
#define	imWavFreq		30	/* Audio sampling freqency */

static
BYTE SndBuff[2048];


/*----------------------------------*/
/* Transfer picture to the display  */

static
int xfer_picture (
	FIL* fp,		/* Pointer to the open file to load */
	void* work,		/* Pointer to the working buffer */
	UINT sz_work,	/* Size of the working buffer [byte] */
	WORD bpp,		/* Color depth [bit] */
	DWORD szpic		/* Picture size [byte] */
)
{
	DWORD n;
	UINT br;
	WORD *dp, w;
	BYTE *bp, b, t;


	do {
		n = (szpic > sz_work) ? sz_work : szpic;
		f_read(fp, work, n, &br);
		if (n != br) return 0;
		szpic -= n;
		switch (bpp) {
		case 16:	/* RGB565 */
			dp = (uint16_t*)work;
			do {
				w = *dp++; DATA_WPX(w);
				w = *dp++; DATA_WPX(w);
			} while (n -= 4);
			break;
		case 8:		/* 256 level grayscale */
			bp = (uint8_t*)work;
			do {
				b = *bp++ & 0xF8;
				w = (b << 8) | (b << 3) | (b >> 3);
				DATA_WPX(w);
				b = *bp++ & 0xF8;
				w = (b << 8) | (b << 3) | (b >> 3);
				DATA_WPX(w);
			} while (n -= 2);
			break;
		default:	/* 16 level grayscale */
			bp = (uint8_t*)work;
			do {
				t = *bp++;
				b = t >> 4; w = (b << 12) | (b << 7) | (b << 1);
				DATA_WPX(w);
				b = t & 0x0F; w = (b << 12) | (b << 7) | (b << 1);
				DATA_WPX(w);
			} while (--n);
		}
	} while (szpic);

	return 1;
}



/*--------------------------------------*/
/* Send audio data to the output stream */

static
int xfer_audio (
	FIL* fp,		/* Pointer to the open file to load */
	void* work,		/* Pointer to the working buffer */
	UINT sz_work,	/* Size of the working buffer [byte] */
	WAVFIFO* pfcb,	/* Pointer to the audio stream fifo control block */
	UINT sz_audio	/* Size of audio (rounded-up to the 512 byte boundary) */
)
{
	UINT br, wi;
	BYTE *wbuf, *rp = work;


	if (sz_audio > sz_work) return 0;
	f_read(fp, work, sz_audio, &br);	/* Load audio data to the buffer */
	if (sz_audio != br) return 0;
	sz_audio = LD_WORD(rp); rp += 2;	/* Get actual size of the audio data */

	/* Push audio data to the audio stream fifo */
	wi = pfcb->wi;
	wbuf = pfcb->buff;
	do {
		if (pfcb->ct < pfcb->sz_buff) {
			wbuf[wi]  = *rp++;
			wi = (wi + 1) & (pfcb->sz_buff - 1);
			IrqDisable();
			pfcb->ct++;
			IrqEnable();
			sz_audio--;
		}
	} while (sz_audio);
	pfcb->wi = wi;

	return 1;
}



/*-----------------------------------*/
/* IMG file loader                   */

void load_img (
	FIL* fp,		/* Pointer to the open file object to load */
	void* work,		/* Pointer to the working buffer (must be 4-byte aligned) */
	UINT sz_work	/* Size of the working buffer (must be power of 2) */
)
{
	char k;
	UINT run, x, y, br, mode;
	WORD bpp;
	long fd, tp;
	DWORD d, sz_pic, sz_frm, nfrm, cfrm;
	BYTE *buff = work;
	WAVFIFO fcb;


	f_read(fp, buff, 128, &br);
	if (br != 128) return;
	mode = 0;
	if (!memcmp(buff+imSign, "IM", 2)) mode = 1;	/* Video only */
	if (!memcmp(buff+imSign, "im", 2)) mode = 2;	/* Audio/Video mixed */
	if (!mode) return;

	x = LD_WORD(buff+imWidth);		/* Check frame size */
	y = LD_WORD(buff+imHeight);
	if (!x || x > DISP_XS || !y || y > DISP_YS) return;

	bpp = LD_WORD(buff+imBpp);		/* Check color depth */
	if (bpp != 16 && bpp != 8 && bpp != 4) return;

	sz_pic = x * y * bpp / 8;	/* Picture size [byte] */

	d = LD_DWORD(buff+imDataOfs);	/* Go to data start position */
	if (f_lseek(fp, d) || f_tell(fp) != d) return;

	disp_fill(0, DISP_XS, 0, DISP_YS, 0);	/* Clear screen */
	disp_setrect((DISP_XS - x) / 2, (DISP_XS - x) / 2 + x - 1, (DISP_YS - y) / 2, (DISP_YS - y) / 2 + y - 1);

	nfrm = LD_DWORD(buff+imFrames);		/* Number of frames */
	cfrm = 0;
	run = 1;

	if (mode == 1) {
		fd = LD_DWORD(buff+imFrmPeriod);	/* Frame period [us] */
		tp = TmrFrm + fd;
		for (;;) {
			if (run && TmrFrm >= tp) {
				if (cfrm >= nfrm) break;	/* End of stream */
				if (!xfer_picture(fp, work, sz_work, bpp, sz_pic)) break;	/* Display picture */
				tp += fd;
				cfrm++;
			}

			k = 0;
			while (uart0_test()) k = uart0_getc();	/* Get button command */
			if (k == BTN_CAN || k == BTN_OK) break;	/* Exit */
			if (k == BTN_UP) {	/* Pause/Resume */
				run ^= 1;
				tp = TmrFrm + fd;
			}
			if (!run) {
				if (k == BTN_RIGHT && cfrm + 1 < nfrm) {	/* Go to next frame */
					if (!xfer_picture(fp, work, sz_work, bpp, sz_pic)) break;	/* Put the picture */
					cfrm++;
				}
				if (k == BTN_LEFT && cfrm >= 2) {	/* Go to previous frame */
					if (f_lseek(fp, f_tell(fp) - sz_pic * 2)) break;
					if (!xfer_picture(fp, work, sz_work, bpp, sz_pic)) break;	/* Put the picture */
					cfrm--;
				}
			}
		}
	} else {		/* Audio/Video mixed stream */
		fcb.mode = LD_WORD(buff+imWavFormat);
		fcb.buff = SndBuff;
		fcb.sz_buff = sizeof SndBuff;
		if (!sound_start(&fcb, LD_WORD(buff+imWavFreq))) return;	/* Open audio output stream */
		sz_frm = LD_DWORD(buff+imFrmSize);

		for (;;) {
			if (run) {
				if (cfrm >= nfrm) break;	/* End of stream */
				if (!xfer_audio(fp, work, sz_work, &fcb, sz_frm - sz_pic)) break;	/* Output audio data */
				if (!xfer_picture(fp, work, sz_work, bpp, sz_pic)) break;			/* Display picture */
				cfrm++;	/* Next frame */
			}

			k = 0;
			while (uart0_test()) k = uart0_getc();		/* Get button command */
			if (k == BTN_CAN || k == BTN_OK) break;	/* Exit */
			if (k == BTN_UP) run ^= 1;
			if (run) continue;
			if (k == BTN_RIGHT && cfrm + 1 < nfrm) {	/* Go to next frame */
				if (f_lseek(fp, f_tell(fp) + sz_frm - sz_pic)) break;		/* Skip audio data */
				if (!xfer_picture(fp, work, sz_work, bpp, sz_pic)) break;	/* Put the picture */
				cfrm += 1;	/* Next frame */
			}
			if (k == BTN_LEFT && cfrm >= 2) {	/* Go to previous frame */
				if (f_lseek(fp, f_tell(fp) - sz_frm * 2 + sz_frm - sz_pic)) break;	/* Goto previous picture */
				if (!xfer_picture(fp, work, sz_work, bpp, sz_pic)) break;			/* Put the picture */
				cfrm -= 2;	/* Previous frame */
			}
		}
		sound_stop();	/* Close sound output stream */
	}
}




/*-----------------------------------*/
/* Windows BMP file loader           */

void load_bmp (
	FIL *fp,		/* Pointer to the open file object to load */
	void *work,		/* Pointer to the working buffer (must be 4-byte aligned) */
	UINT sz_work	/* Size of the working buffer (must be power of 2) */
)
{
	DWORD n, m, biofs, bm_w, bm_h, iw, w, h, lc, left, top, xs, xe, ye;
	UINT bx;
	BYTE *buff = work, *p, k;
	WORD d;


	f_read(fp, buff, 128, &bx);
	if (bx != 128 || memcmp(buff, "BM", 2)) return;
	biofs = LD_DWORD(buff+10);			/* bfOffBits */
	if (LD_WORD(buff+26) != 1) return;	/* biPlanes */
	if (LD_WORD(buff+28) != 24) return;	/* biBitCount */
	if (LD_DWORD(buff+30) != 0) return;	/* biCompression */
	bm_w = LD_DWORD(buff+18);			/* biWidth */
	bm_h = LD_DWORD(buff+22);			/* biHeight */
	iw = ((bm_w * 3) + 3) & ~3;			/* Bitmap line stride [byte] */
	if (!bm_w || !bm_h) return;			/* Check bitmap size */
	if (iw > sz_work) return;			/* Check if buffer size is sufficient for this file */

	disp_fill(0, DISP_XS, 0, DISP_YS, 0);	/* Clear screen */
	/* Determine left/right of image rectangular */
	if (bm_w > DISP_XS) {
		xs = 0; xe = DISP_XS - 1;	/* Full width */
	} else {
		xs = (DISP_XS - bm_w) / 2;	/* H-centering */
		xe = (DISP_XS - bm_w) / 2 + bm_w - 1;
	}
	/* Determine top/bottom of image rectangular */
	if (bm_h > DISP_YS) {	/* Full height */
		ye = DISP_YS - 1;
	} else {				/* V-centering */
		ye = (DISP_YS - bm_h) / 2 + bm_h - 1;
	}

	left = top = 0;	/* Offset from left/top of the picture */
	do {
		/* Put a rectangular of the picture */
		m = (bm_h <= DISP_YS) ? biofs : biofs + (bm_h - DISP_YS - top) * iw;
		if (f_lseek(fp, m) || m != f_tell(fp)) break;	/* Goto bottom line of the window */
		w = (bm_w > DISP_XS) ? DISP_XS : bm_w;	/* Rectangular width [pix] */
		h = (bm_h > DISP_YS) ? DISP_YS : bm_h;	/* Rectangular height [pix] */
		m = ye;
		do {
			lc = sz_work / iw;	/* Get some lines fit in the working buffer */
			if (lc > h) lc = h;
			h -= lc;
			f_read(fp, buff, lc * iw, &bx);
			disp_setrect(xs, xe, m - lc + 1, m);	/* Begin to transfer data to a rectangular area */
			m -= lc;
			do {	/* Line loop */
				lc--; p = &buff[lc * iw + left * 3];
				n = w;
				do {	/* Pixel loop  */
					d = *p++ >> 3;	/* Get an RGB888 pixel, convert to RGB565 format */
					d |= (*p++ >> 2) << 5;
					d |= (*p++ >> 3) << 11;
					DATA_WPX(d);
				} while (--n);
			} while (lc);
		} while (h);

		k = uart0_getc();	/* Get key command */
		while (uart0_test()) uart0_getc();	/* Flush command queue */
		switch (k) {
		case BTN_RIGHT:	/* Move right */
			if (bm_w > DISP_XS)
				left = (left + MOVE_X + DISP_XS <= bm_w) ? left + MOVE_X : bm_w - DISP_XS;
			break;
		case BTN_LEFT:	/* Move left */
			if (bm_w > DISP_XS)
				left = (left >= MOVE_X) ? left - MOVE_X : 0;
			break;
		case BTN_DOWN:	/* Move down */
			if (bm_h > DISP_YS)
				top = (top + DISP_YS + MOVE_Y <= bm_h) ? top + MOVE_Y : bm_h - DISP_YS;
			break;
		case BTN_UP:	/* Move up */
			if (bm_h > DISP_YS)
				top = (top >= MOVE_Y) ? top - MOVE_Y : 0;
			break;
		default:		/* Exit */
			k = 0;
		}
	} while (k);
}



/*-----------------------------------*/
/* JPEG file loader                  */


static
UINT tjd_input (
	JDEC* jd,		/* Decompression object */
	BYTE* buff,		/* Pointer to the read buffer (NULL:skip) */
	UINT nd			/* Number of bytes to read/skip from input stream */
)
{
	UINT rb;
	FIL *fil = (FIL*)jd->device;	/* Input stream of this session */


	if (buff) {	/* Read nd bytes from the input stream */
		f_read(fil, buff, nd, &rb);
		return rb;

	} else {	/* Skip nd bytes on the input stream */
		return (f_lseek(fil, f_tell(fil) + nd) == FR_OK) ? nd : 0;
	}
}



static
UINT tjd_output (
	JDEC* jd,		/* Decompression object */
	void* bitmap,	/* Bitmap data to be output */
	JRECT* rect		/* Rectangular region to output */
)
{
	jd = jd;	/* (suppress warning) */

	if (!rect->left && uart0_test()) return 0;	/* Check user interrupt at left end */

	disp_blt(rect->left, rect->right, rect->top, rect->bottom, (uint16_t*)bitmap);

	return 1;
}




void load_jpg (
	FIL *fp,		/* Pointer to the open file object to load */
	void *work,		/* Pointer to the working buffer (must be 4-byte aligned) */
	UINT sz_work	/* Size of the working buffer (must be power of 2) */
)
{
	JDEC jd;
	JRESULT rc;
	BYTE scale;


	disp_fill(0, DISP_XS, 0, DISP_YS, 0);	/* Clear screen */
	disp_font_color(C_WHITE);

	rc = jd_prepare(&jd, tjd_input, work, sz_work, fp);		/* Prepare to decompress the file */
	if (rc == JDR_OK) {
		for (scale = 0; scale < 3; scale++) {
			if ((jd.width >> scale) <= DISP_XS && (jd.height >> scale) <= DISP_YS) break;
		}
		disp_locate(0, TS_HEIGHT - 1);
		xfprintf(disp_putc, "%ux%u 1/%u", jd.width, jd.height, 1 << scale);

		rc = jd_decomp(&jd, tjd_output, scale);	/* Start to decompress */
	} else {
		disp_locate(0, 0);
		xfprintf(disp_putc, "Error: %d", rc);
	}
	uart0_getc();
}




/*-----------------------------------*/
/* Text file loader                  */

typedef struct {
	BYTE sbuf[512];
	DWORD ltbl[1];
} TXVIEW;


void load_txt (
	FIL *fp,		/* Pointer to the open file object to load */
	void *work,
	UINT sz_work
)
{
	TXVIEW *tv = work;
	UINT lw, i, j, line, br, col, lines, max_lines;
	DWORD cfp;
	BYTE c;
	char k;


	max_lines = (sz_work - (UINT)tv->ltbl + (UINT)&tv) / 4;

	cfp = br = i = 0;
	tv->ltbl[0] = 0;
	for (line = 0; line < max_lines - 1; ) {
		if (i >= br) {
			cfp = f_tell(fp);
			f_read(fp, tv->sbuf, 512, &br);
			if (br == 0) break;
			i = 0;
		}
		if (tv->sbuf[i++] == '\n')
			tv->ltbl[++line] = cfp + i;
	}
	lines = line;
	line = col = 0;
	disp_font_color(C_WHITE);
	for (;;) {
		disp_locate(0, 0);
		for (i = line; i < line + TS_HEIGHT && i < lines; i++) {
			f_lseek(fp, tv->ltbl[i]);
			lw = tv->ltbl[i + 1] - tv->ltbl[i];
			f_read(fp, tv->sbuf, lw > 512 ? 512 : lw, &br);
			if (!br) return;
			for (j = 0; j < col && tv->sbuf[j] != '\n'; j++) {
#if DISP_USE_DBCS
				if (FontD && ((tv->sbuf[j] >= 0x81 && tv->sbuf[j] <= 0x9F) || (tv->sbuf[j] >= 0xE0 && tv->sbuf[j] <= 0xFC))) j++;
#endif
			}
			if (tv->sbuf[j] != '\n') {
				if (j > col) disp_putc(' ');
				for ( ; tv->sbuf[j] != '\n' && j < TS_WIDTH + col; j++) {
					c = tv->sbuf[j];
					if (c == '\r' || c == '\t') c = ' ';
					disp_putc(c);
				}
			}
			for ( ; j < TS_WIDTH + col; j++) disp_putc(' ');
			disp_putc('\n');
		}
		for ( ; i < line + TS_HEIGHT; i++) {
			for (j = 0; j < TS_WIDTH; j++) disp_putc(' ');
			disp_putc('\n');
		}

		k = uart0_getc();
		if (k == BTN_CAN || k== BTN_OK) break;
		if (k == BTN_DOWN) {
			if (line + TS_HEIGHT < lines) line++;
		}
		if (k == BTN_UP) {
			if (line) line--;
		}
		if (k == BTN_LEFT) {
			if (col) col--;
		}
		if (k == BTN_RIGHT) {
			if (col < 512 - col) col++;
		}
	}
}


#endif

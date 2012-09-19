/*------------------------------------------------*/
/* SCI and Monitor I/O functions                  */


#include <machine.h>
#include <stdarg.h>
#include "iodefine.h"
#include "monitor.h"
#include "integer.h"



typedef struct _fifo {
	BYTE	idx_w;
	BYTE	idx_r;
	BYTE	count;
	BYTE buff[64];
} FIFO;


static volatile
FIFO txfifo, rxfifo;



/* Initialize SCI3 */

void sci3_init()
{
	WORD n;


	rxfifo.idx_r = 0;
	rxfifo.idx_w = 0;
	rxfifo.count = 0;
	txfifo.idx_r = 0;
	txfifo.idx_w = 0;
	txfifo.count = 0;

	SCI3.SCR3.BYTE = 0;
	SCI3.SMR.BYTE = 0;		// N81
	SCI3.BRR = 15;			// 38400bps @ 20MHz
	IO.PMR1.BIT.TXD = 1;
	for (n = 0; n < 10000; n++);
	SCI3.SCR3.BYTE = 0xF0;	// TE = TIE = RE = RIE = 1;
}



char xgetc ()
{
	BYTE d, i;


	i = rxfifo.idx_r;
	while(rxfifo.count == 0);
	d = rxfifo.buff[i++];
	set_imask_ccr(1);
	rxfifo.count--;
	set_imask_ccr(0);
	if(i >= sizeof(rxfifo.buff))
		i = 0;
	rxfifo.idx_r = i;

	return d;
}



/* Put a character  */

void xputc (char d)
{
	BYTE i;

	if (d == '\n') xputc('\r');
	i = txfifo.idx_w;
	while(txfifo.count >= sizeof(txfifo.buff));
	txfifo.buff[i++] = d;
	set_imask_ccr(1);
	txfifo.count++;
	SCI3.SCR3.BIT.TIE = 1;
	set_imask_ccr(0);
	if(i >= sizeof(txfifo.buff))
		i = 0;
	txfifo.idx_w = i;
}



/* SCI3 interrupt */

void int_SCI3 ()
{
	BYTE stat, d, n, i;


	stat = SCI3.SSR.BYTE;
	if (stat & 0x38) {
		stat &= ~0x38;
		SCI3.SSR.BYTE = stat;
	}
	if (stat & 0x40) {
		d = SCI3.RDR;
		n = rxfifo.count;
		if(n < sizeof(rxfifo.buff)) {
			rxfifo.count = ++n;
			i = rxfifo.idx_w;
			rxfifo.buff[i++] = d;
			if(i >= sizeof(rxfifo.buff))
				i = 0;
			rxfifo.idx_w = i;
		}
	}
	if (stat & 0x80) {
		n = txfifo.count;
		if(n) {
			txfifo.count = --n;
			i = txfifo.idx_r;
			SCI3.TDR = txfifo.buff[i++];
			if(i >= sizeof(txfifo.buff))
				i = 0;
			txfifo.idx_r = i;
		}
		if(n == 0)
			SCI3.SCR3.BIT.TIE = 0;
	}
}




char xatoi (char **str, long *res)
{
	DWORD val;
	BYTE c, radix, s = 0;


	while ((c = **str) == ' ') (*str)++;
	if (c == '-') {
		s = 1;
		c = *(++(*str));
	}
	if (c == '0') {
		c = *(++(*str));
		if (c <= ' ') {
			*res = 0; return 1;
		}
		if (c == 'x') {
			radix = 16;
			c = *(++(*str));
		} else {
			if (c == 'b') {
				radix = 2;
				c = *(++(*str));
			} else {
				if ((c >= '0')&&(c <= '9'))
					radix = 8;
				else
					return 0;
			}
		}
	} else {
		if ((c < '1')||(c > '9'))
			return 0;
		radix = 10;
	}
	val = 0;
	while (c > ' ') {
		if (c >= 'a') c -= 0x20;
		c -= '0';
		if (c >= 17) {
			c -= 7;
			if (c <= 9) return 0;
		}
		if (c >= radix) return 0;
		val = val * radix + c;
		c = *(++(*str));
	}
	if (s) val = -val;
	*res = val;
	return 1;
}



void xputs (const char* str)
{
	char d;


	while ((d = *str++) != 0)
		xputc(d);
}



void xitoa (long val, char radix, char len)
{
	BYTE c, r, sgn = 0, pad = ' ';
	BYTE s[20], i = 0;
	DWORD value = val;



	if (radix < 0) {
		radix = -radix;
		if (val < 0) { 
			val = -val;
			sgn = '-';
		}
	}
	r = radix;
	if (len < 0) {
		len = -len;
		pad = '0';
	}
	if (len > 20) return;
	do {
		c = (BYTE)(value % r);
		if (c >= 10) c += 7;
		c += '0';
		s[i++] = c;
		value /= r;
	} while (value);
	if (sgn) s[i++] = sgn;
	while (i < len)
		s[i++] = pad;
	do
		xputc(s[--i]);
	while (i);
}



void xprintf (const char* str, ...)
{
	va_list arp;
	char d, r, w, s, l;


	va_start(arp, str);

	while ((d = *str++) != 0) {
		if (d != '%') {
			xputc(d); continue;
		}
		d = *str++; w = r = s = l = 0;
		if (d == '0') {
			d = *str++; s = 1;
		}
		while ((d >= '0')&&(d <= '9')) {
			w += w * 10 + (d - '0');
			d = *str++;
		}
		if (s) w = -w;
		if (d == 'l') {
			l = 1;
			d = *str++;
		}
		if (!d) break;
		if (d == 's') {
			xputs((char*)va_arg(arp, char*));
			continue;
		}
		if (d == 'c') {
			xputc(va_arg(arp, short));
			continue;
		}
		if (d == 'u') r = 10;
		if (d == 'd') r = -10;
		if (d == 'X') r = 16;
		if (d == 'b') r = 2;
		if (!r) break;
		if (l) {
			xitoa((long)va_arg(arp, long), r, w);
		} else {
			if (r > 0)
				xitoa((unsigned long)va_arg(arp, short), r, w);
			else
				xitoa((long)va_arg(arp, short), r, w);
		}
	}

	va_end(arp);
}



void get_line (char *buff, int len)
{
	char c;
	BYTE idx = 0;


	for (;;) {
		c = xgetc();
		if (c == '\r') break;
		if ((c == '\b') && idx) {
			idx--; xputc(c);
		}
		if (((BYTE)c >= ' ') && (idx < len - 1)) {
				buff[idx++] = c; xputc(c);
		}
	}
	buff[idx] = 0;
	xputc('\n');
}

/*------------------------------------------------------------------------*/
/* Universal string handler for user console interface  (C)ChaN, 2010     */
/*------------------------------------------------------------------------*/

#ifndef _STRFUNC
#define _STRFUNC

#define _USE_XFUNC_OUT	1	/* 1: Enable xputc/xputs/xprintf/xsprintf/xdprintf function */
#define _USE_DUMP		1	/* 1: Enable put_dump function */
#define	_CR_CRLF		1	/* 1: Convert \n ==> \r\n in the output char */

#define _USE_XFUNC_IN	1	/* 1: Enable xatoi/get_line function */
#define	_LINE_ECHO		1	/* 1: Echo back input chars in get_line function */


#ifdef __cplusplus
extern "C" {
#endif

#if _USE_XFUNC_OUT
#define xdev_out(func) xfunc_out = (void(*)(unsigned char))(func)
extern void (*xfunc_out)(unsigned char);
void xputc (char c);
void xputs (const char* str);
void xprintf (const char* fmt, ...);
void xsprintf (char* buff, const char* fmt, ...);
void xdprintf (void (*func)(unsigned char), const char*	fmt, ...);
void put_dump (const void* buff, unsigned long addr, int len);
#endif

#if _USE_XFUNC_IN
#define xdev_in(func) xfunc_in = (unsigned char(*)(void))(func)
extern unsigned char (*xfunc_in)(void);
int get_line (char* buff, int len);
int xatoi (char** str, long* res);
#endif

#ifdef __cplusplus
}
#endif

#endif

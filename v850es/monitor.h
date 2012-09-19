#include "integer.h"
#include "comm.h"
#define xgetc()		(char)uart_get()

int xatoi (const char**, long*);
void xputc (char);
void xputs (const char*);
void xitoa (long, char, char);
void xprintf (const char*, ...);
void put_dump (const BYTE*, DWORD ofs, int cnt);
void get_line (char*, int len);


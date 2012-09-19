void uart_init();			/* Initialize UART and Flush FIFOs */
char xgetc ();			/* Get a byte from UART Rx FIFO */
char uart_test();			/* Check number of data in UART Rx FIFO */
void xputc (char);		/* Put a byte into UART Tx FIFO */

void xputs (const char*);
void xitoa (long, char, char);
void xprintf (const char*, ...);
char xatoi (char**, long*);
void get_line (char*, int);

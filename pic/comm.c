#include <string.h>
#include <p24FJ64GA002.h>
#include "pic24f.h"
#include "comm.h"

#define BUFFER_SIZE 128
#define BPS 		115200UL


static int TxRun;
static volatile struct
{
	int		rptr;
	int		wptr;
	int		count;
	BYTE	buff[BUFFER_SIZE];
} TxFifo, RxFifo;




void __attribute__((interrupt, auto_psv)) _U1RXInterrupt (void)
{
	BYTE d;
	int n;


	d = (BYTE)U1RXREG;
	_U1RXIF = 0;
	n = RxFifo.count;
	if (n < BUFFER_SIZE) {
		n++;
		RxFifo.count = n;
		n = RxFifo.wptr;
		RxFifo.buff[n] = d;
		RxFifo.wptr = (n + 1) % BUFFER_SIZE;
	}
}




void __attribute__((interrupt, auto_psv)) _U1TXInterrupt (void)
{
	int n;


	_U1TXIF = 0;
	n = TxFifo.count;
	if (n) {
		n--;
		TxFifo.count = n;
		n = TxFifo.rptr;
		U1TXREG = TxFifo.buff[n];
		TxFifo.rptr = (n + 1) % BUFFER_SIZE;
	} else {
		TxRun = 0;
	}
}




int uart_test (void)
{
	return RxFifo.count;
}




BYTE uart_get (void)
{
	BYTE d;
	int n;


	while (!RxFifo.count);

	n = RxFifo.rptr;
	d = RxFifo.buff[n];
	RxFifo.rptr = (n + 1) % BUFFER_SIZE;
	_DI();
	RxFifo.count--;
	_EI();

	return d;
}




void uart_put (BYTE d)
{
	int n;


	while (TxFifo.count >= BUFFER_SIZE);

	n = TxFifo.wptr;
	TxFifo.buff[n] = d;
	TxFifo.wptr = (n + 1) % BUFFER_SIZE;
	_DI();
	TxFifo.count++;
	if (!TxRun) {
		TxRun = 1;
		_U1TXIF = 1;
	}
	_EI();
}




void uart_init (void)
{
	/* Disable Tx/Rx interruptrs */
	_U1RXIE = 0;
	_U1TXIE = 0;

	/* Initialize UART1 */
	U1BRG = FCY / 16 / BPS - 1;
	_UARTEN = 1;
	_UTXEN = 1;

	/* Clear Tx/Rx FIFOs */
	TxFifo.rptr = TxFifo.wptr = TxFifo.count = 0;
	RxFifo.rptr = RxFifo.wptr = RxFifo.count = 0;
	TxRun = 0;

	/* Enable Tx/Rx interruptrs */
	_U1RXIE = 1;
	_U1TXIE = 1;
}



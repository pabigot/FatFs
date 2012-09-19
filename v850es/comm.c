#include <string.h>
#include "comm.h"


#define BUFFER_SIZE 128
#define BPS 		115200


static volatile int TxRun;
static volatile struct
{
	int		rptr;
	int		wptr;
	int		count;
	BYTE	buff[BUFFER_SIZE];
} TxFifo, RxFifo;




#pragma interrupt INTUA0R ISR_uart_rcvr
void ISR_uart_rcvr (void)
{
	BYTE d;
	int n;


	d = UA0RX;
	n = RxFifo.count;
	if (n < BUFFER_SIZE) {
		n++;
		RxFifo.count = n;
		n = RxFifo.wptr;
		RxFifo.buff[n] = d;
		RxFifo.wptr = (n + 1) % BUFFER_SIZE;
	}
}




#pragma interrupt INTUA0T ISR_uart_xmit
void ISR_uart_xmit (void)
{
	int n;


	n = TxFifo.count;
	if (n) {
		n--;
		TxFifo.count = n;
		n = TxFifo.rptr;
		UA0TX = TxFifo.buff[n];
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


	do; while (!RxFifo.count);

	n = RxFifo.rptr;
	d = RxFifo.buff[n];
	RxFifo.rptr = (n + 1) % BUFFER_SIZE;
	__DI();
	RxFifo.count--;
	__EI();

	return d;
}




void uart_put (BYTE d)
{
	int n;


	do; while (TxFifo.count >= BUFFER_SIZE);

	n = TxFifo.wptr;
	TxFifo.buff[n] = d;
	TxFifo.wptr = (n + 1) % BUFFER_SIZE;
	__DI();
	TxFifo.count++;
	if (!TxRun) {
		TxRun = 1;
		UA0TIF = 1;
	}
	__EI();
}




void uart_init (void)
{
	UA0RMK = 1;
	UA0TMK = 1;

	/* Attach UARTA0 unit to I/O pin */
	P3L |= 0x01;
	PM3L &= ~0x01;
	PMC3L |= 0x03;

	/* Initialize UARTA0 */
	UA0CTL0 = 0x92;
	UA0CTL1 = 0;
	UA0CTL2 = SYSCLK / 2 / BPS;
	UA0CTL0 = 0xF2;

	/* Clear Tx/Rx FIFOs */
	TxFifo.rptr = TxFifo.wptr = TxFifo.count = 0;
	RxFifo.rptr = RxFifo.wptr = RxFifo.count = 0;
	TxRun = 0;

	/* Enable Tx/Rx interruptrs */
	UA0RMK = 0;
	UA0TMK = 0;
}



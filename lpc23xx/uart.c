/*------------------------------------------------------------------------/
/  LPC2300 UART control module
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

#include "uart.h"
#include "interrupt.h"



#define UART_CH		0			/* UART channel number 0-3 */
#define UART_BPS 	230400		/* Bit rate */
#define FIFO_SIZE	128			/* Length of Receive/Transmission FIFO */
#define	CCLK		72000000	/* cclk frequency */
#define PCLK		18000000	/* PCLK frequency for the UART module */
#define	DIVADD		5			/* See below */
#define	MULVAL		8

/* PCLK      8/16M  9/18M 10/20M 12/24M 12.5/25M 15/30M */
/* DIVADD       1     5      1      1      1       5    */
/* MULVAL      12     8     12     12      8       8    */
/* Error[%]   0.15  0.16   0.15   0.15   0.47    0.16   */

#define	DLVAL		((unsigned int)((double)PCLK / UART_BPS / 16 / (1 + (double)DIVADD / MULVAL)))

#if PCLK * 1 == CCLK
#define PCLKDIV	PCLKDIV_1
#elif PCLK * 2 == CCLK
#define PCLKDIV	PCLKDIV_2
#elif PCLK * 4 == CCLK
#define PCLKDIV	PCLKDIV_4
#elif PCLK * 8 == CCLK
#define PCLKDIV	PCLKDIV_8
#else
#error Invalid frequency combination
#endif

#if UART_CH == 0
#define PCUART		PCUART0
#define PCLK_UART	PCLK_UART0
#define UART_THR	U0THR
#define UART_RBR	U0RBR
#define UART_LSR	U0LSR
#define UART_LCR	U0LCR
#define UART_DLM	U0DLM
#define UART_DLL	U0DLL
#define UART_FDR	U0FDR
#define UART_LCR	U0LCR
#define UART_FCR	U0FCR
#define UART_TER	U0TER
#define UART_IIR	U0IIR
#define UART_IER	U0IER
#define	UART_IRQn	UART0_IRQn
#define	UART_IRQHandler	UART0_IRQHandler
#define	ATTACH_UART() {\
	__set_PINSEL(0, 3, 1);	/* P0.3 - RXD0 */\
	__set_PINSEL(0, 2, 1);	/* P0.2 - TXD0 */\
	FIO0DIR0 |= _BV(2);		/* P0.2 - output */\
}
#elif UART_CH == 1
#define PCUART		PCUART1
#define PCLK_UART	PCLK_UART1
#define UART_THR	U1THR
#define UART_RBR	U1RBR
#define UART_LSR	U1LSR
#define UART_LCR	U1LCR
#define UART_DLM	U1DLM
#define UART_DLL	U1DLL
#define UART_FDR	U1FDR
#define UART_LCR	U1LCR
#define UART_FCR	U1FCR
#define UART_TER	U1TER
#define UART_IIR	U1IIR
#define UART_IER	U1IER
#define	UART_IRQn	UART1_IRQn
#define	UART_IRQHandler	UART1_IRQHandler
#define	ATTACH_UART() {\
	__set_PINSEL(0, 16, 1);	/* P0.16 - RXD1 */\
	__set_PINSEL(0, 15, 1);	/* P0.15 - TXD1 */\
	FIO0DIR1 |= _BV(7);		/* P0.15 - output */\
}
#elif UART_CH == 2
#define PCUART		PCUART2
#define PCLK_UART	PCLK_UART2
#define UART_THR	U2THR
#define UART_RBR	U2RBR
#define UART_LSR	U2LSR
#define UART_LCR	U2LCR
#define UART_DLM	U2DLM
#define UART_DLL	U2DLL
#define UART_FDR	U2FDR
#define UART_LCR	U2LCR
#define UART_FCR	U2FCR
#define UART_TER	U2TER
#define UART_IIR	U2IIR
#define UART_IER	U2IER
#define	UART_IRQn	UART2_IRQn
#define	UART_IRQHandler	UART2_IRQHandler
#define	ATTACH_UART() {\
	__set_PINSEL(0, 11, 1);	/* P0.11 - TXD2 */\
	__set_PINSEL(0, 10, 1);	/* P0.10 - RXD2 */\
	FIO0DIR1 |= _BV(2);		/* P0.10 - output */\
}
#elif UART_CH == 3
#define PCUART		PCUART3
#define PCLK_UART	PCLK_UART3
#define UART_THR	U3THR
#define UART_RBR	U3RBR
#define UART_LSR	U3LSR
#define UART_LCR	U3LCR
#define UART_DLM	U3DLM
#define UART_DLL	U3DLL
#define UART_FDR	U3FDR
#define UART_LCR	U3LCR
#define UART_FCR	U3FCR
#define UART_TER	U3TER
#define UART_IIR	U3IIR
#define UART_IER	U3IER
#define	UART_IRQn	UART3_IRQn
#define	UART_IRQHandler	UART3_IRQHandler
#define	ATTACH_UART() {\
	__set_PINSEL(0, 1, 2);	/* P0.1 - TXD3 */\
	__set_PINSEL(0, 0, 2);	/* P0.0 - RXD3 */\
	FIO0DIR0 |= _BV(0);		/* P0.0 - output */\
}
#else
#error Invalid UART channel.
#endif


static volatile struct {
	uint16_t	ri, wi, ct, act;
	uint8_t		buff[FIFO_SIZE];
} TxBuff, RxBuff;



void Isr_UART (void)
{
	uint8_t iir, d;
	uint16_t i, cnt;


	for (;;) {
		iir = UART_IIR;			/* Get Interrupt ID*/
		if (iir & 1) break;		/* Exit if there is no interrupt */
		switch (iir & 7) {
		case 4:			/* Receive FIFO is half filled or timeout occured */
			i = RxBuff.wi;
			cnt = RxBuff.ct;
			while (UART_LSR & 0x01) {	/* Get all data in the Rx FIFO */
				d = UART_RBR;
				if (cnt < FIFO_SIZE) {	/* Store data if Rx buffer is not full */
					RxBuff.buff[i++] = d;
					i %= FIFO_SIZE;
					cnt++;
				}
			}
			RxBuff.wi = i;
			RxBuff.ct = cnt;
			break;

		case 2:			/* Transmisson FIFO empty */
			cnt = TxBuff.ct;
			if (cnt) {
				i = TxBuff.ri;
				for (d = 16; d && cnt; d--, cnt--) {	/* Fill Tx FIFO */
					UART_THR = TxBuff.buff[i++];
					i %= FIFO_SIZE;
				}
				TxBuff.ri = i;
				TxBuff.ct = cnt;
			} else {
				TxBuff.act = 0; /* No data to send. Next putc must trigger Tx sequense */
			}
			break;

		default:		/* Data error or break detected */
			d = UART_LSR;
			d = UART_RBR;
			break;
		}
	}
}




int uart_test (void)
{
	return RxBuff.ct;
}




uint8_t uart_getc (void)
{
	uint8_t d;
	uint16_t i;

	/* Wait while Rx buffer is empty */
	while (!RxBuff.ct) ;

	UART_IER = 0;			/* Disable interrupts */
	i = RxBuff.ri;
	d = RxBuff.buff[i++];	/* Get a byte from Rx buffer */
	RxBuff.ri = i % FIFO_SIZE;
	RxBuff.ct--;
	UART_IER = 0x07;		/* Reenable interrupt */

	return d;
}




void uart_putc (
	uint8_t d
)
{
	uint16_t i;

	/* Wait for buffer ready */
	while (TxBuff.ct >= FIFO_SIZE) ;

	UART_IER = 0x05;		/* Disable Tx Interrupt */
	if (TxBuff.act) {		/* When not in runnig, trigger transmission */
		i = TxBuff.wi;		/* Put a byte into Tx byffer */
		TxBuff.buff[i++] = d;
		TxBuff.wi = i % FIFO_SIZE;
		TxBuff.ct++;
	} else {
		UART_THR = d;		/* Trigger Tx sequense */
		TxBuff.act = 1;
	}
	UART_IER = 0x07;		/* Reenable Tx Interrupt */
}




void uart_init (void)
{
	UART_IER = 0x00;

	/* Enable UART module and set PCLK frequency */
	__set_PCONP(PCUART, 1);
	__set_PCLKSEL(PCLK_UART, PCLKDIV);

	/* Initialize UART */
	UART_LCR = 0x83;			/* Select divisor latch */
	UART_DLM = DLVAL / 256;		/* Initialize BRG */
	UART_DLL = DLVAL % 256;
	UART_FDR = (MULVAL << 4) | DIVADD;
	UART_LCR = 0x03;			/* Set serial format N81 and deselect divisor latch */
	UART_FCR = 0x87;			/* Enable FIFO */
	UART_TER = 0x80;			/* Enable Tansmission */

	/* Attach UART to I/O pad */
	ATTACH_UART();

	/* Clear Tx/Rx FIFOs */
	TxBuff.ri = 0; TxBuff.wi = 0; TxBuff.ct = 0; TxBuff.act = 0;
	RxBuff.ri = 0; RxBuff.wi = 0; RxBuff.ct = 0;

	/* Enable Tx/Rx/Error interrupts */
	RegisterIrq(UART_IRQn, Isr_UART, PRI_LOWEST);
	UART_IER = 0x07;
}



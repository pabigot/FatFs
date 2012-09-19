/*------------------------------------------------------------------------/
/  LPC1700 UART control module
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


#define UART_CH		0			/* UART channel number 0-3 */
#define UART_BPS 	230400		/* Bit rate */
#define BUFF_SIZE	128			/* Length of Receive/Transmission FIFO */
#define	CCLK		100000000	/* cclk frequency */
#define PCLK		25000000	/* PCLK frequency for the UART module */
#define	DIVADD		1			/* See below */
#define	MULVAL		8

/* PCLK      8/16M  9/18M 10/20M 12/24M 12.5/25M 15/30M */
/* DIVADD       1     5      1      1      1       5    */
/* MULVAL      12     8     12     12      8       8    */
/* Error[%]   0.15  0.16   0.15   0.15   0.47    0.16   */

#define	DLVAL		((uint32_t)((double)PCLK / UART_BPS / 16 / (1 + (double)DIVADD / MULVAL)))

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
	uint8_t		buff[BUFF_SIZE];
} TxBuff, RxBuff;



void UART_IRQHandler (void)
{
	uint8_t iir, d;
	int i, cnt;


	for (;;) {
		iir = UART_IIR;			/* Get interrupt ID */
		if (iir & 1) break;		/* Exit if there is no interrupt */
		switch (iir & 7) {
		case 4:			/* Rx FIFO is half filled or timeout occured */
			i = RxBuff.wi;
			cnt = RxBuff.ct;
			while (UART_LSR & 0x01) {	/* Get all data in the Rx FIFO */
				d = UART_RBR;
				if (cnt < BUFF_SIZE) {	/* Store data if Rx buffer is not full */
					RxBuff.buff[i++] = d;
					i %= BUFF_SIZE;
					cnt++;
				}
			}
			RxBuff.wi = i;
			RxBuff.ct = cnt;
			break;

		case 2:			/* Tx FIFO empty */
			cnt = TxBuff.ct;
			if (cnt) {		/* There is one or more byte to send */
				i = TxBuff.ri;
				for (d = 16; d && cnt; d--, cnt--) {	/* Fill Tx FIFO */
					UART_THR = TxBuff.buff[i++];
					i %= BUFF_SIZE;
				}
				TxBuff.ri = i;
				TxBuff.ct = cnt;
			} else {
				TxBuff.act = 0; /* When no data to send, next putc must trigger Tx sequense */
			}
			break;

		default:		/* Data error or break detected */
			UART_LSR;
			UART_RBR;
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
	int i;

	/* Wait while Rx buffer is empty */
	while (!RxBuff.ct) ;

	i = RxBuff.ri;			/* Get a byte from Rx buffer */
	d = RxBuff.buff[i++];
	RxBuff.ri = i % BUFF_SIZE;
	__disable_irq();
	RxBuff.ct--;
	__enable_irq();

	return d;
}



void uart_putc (uint8_t d)
{
	int i;

	/* Wait for Tx buffer ready */
	while (TxBuff.ct >= BUFF_SIZE) ;

	__disable_irq();
	if (TxBuff.act) {
		i = TxBuff.wi;		/* Put a byte into Tx byffer */
		TxBuff.buff[i++] = d;
		TxBuff.wi = i % BUFF_SIZE;
		TxBuff.ct++;
	} else {
		UART_THR = d;		/* Trigger Tx sequense */
		TxBuff.act = 1;
	}
	__enable_irq();
}



void uart_init (void)
{
	__disable_irqn(UART_IRQn);

	/* Enable UART module and set PCLK frequency */
	__set_PCONP(PCUART, 1);
	__set_PCLKSEL(PCLK_UART, PCLKDIV);

	/* Initialize UART0 */
	UART_LCR = 0x83;			/* Select baud rate divisor latch */
	UART_DLM = DLVAL / 256;		/* Set BRG dividers */
	UART_DLL = DLVAL % 256;
	UART_FDR = (MULVAL << 4) | DIVADD;
	UART_LCR = 0x03;			/* Set serial format N81 and deselect divisor latch */
	UART_FCR = 0x87;			/* Enable FIFO */
	UART_TER = 0x80;			/* Enable Tansmission */

	/* Clear Tx/Rx buffers */
	TxBuff.ri = 0; TxBuff.wi = 0; TxBuff.ct = 0; TxBuff.act = 0;
	RxBuff.ri = 0; RxBuff.wi = 0; RxBuff.ct = 0;

	/* Attach UART to I/O pad */
	ATTACH_UART();

	/* Enable Tx/Rx/Error interrupts */
	UART_IER = 0x07;
	__enable_irqn(UART_IRQn);
}



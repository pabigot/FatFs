/*------------------------------------------------------------------------/
/  MB9BF616/617/618 UART control module
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2012, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/

#include "FM3_type2.h"
#include "uart.h"


#define UART_MFS	3			/* MFS channel to be used as UART (0-3,4-7) */
#define UART_LOC	2			/* Pin relocation number (0-2) */
#define UART_BPS 	115200		/* Bit rate */
#define F_PCLK		72000000	/* Bus clock for the MFS module */

#define BUFF_SIZE	128			/* Depth of Rx/Tx FIFO */


/*--------------------------------------------------------------------------

   Module Private Functions and Macros

---------------------------------------------------------------------------*/

#if   UART_MFS == 0
#define MFS_RX_IRQHandler MFS0_RX_IRQHandler
#define MFS_TX_IRQHandler MFS0_TX_IRQHandler
#define MFS_RX_IRQn MFS0_RX_IRQn
#define MFS_TX_IRQn MFS0_TX_IRQn
#define MFS_SMR   MFS0_SMR
#define MFS_SCR   MFS0_SCR
#define MFS_ESCR  MFS0_ESCR
#define MFS_SSR   MFS0_SSR
#define MFS_TDR   MFS0_TDR
#define MFS_RDR   MFS0_RDR
#define MFS_BGR   MFS0_BGR
#if   UART_LOC == 0
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 4)) | (4 << 4); PFR2 |= 3 << 1; }
#elif UART_LOC == 1
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 4)) | (10 << 4); PFR1 |= 3 << 4; }
#elif UART_LOC == 2
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 4)) | (15 << 4); PFRB |= 3 << 4; }
#endif

#elif UART_MFS == 1
#define MFS_RX_IRQHandler MFS1_RX_IRQHandler
#define MFS_TX_IRQHandler MFS1_TX_IRQHandler
#define MFS_RX_IRQn MFS1_RX_IRQn
#define MFS_TX_IRQn MFS1_TX_IRQn
#define MFS_SMR   MFS1_SMR
#define MFS_SCR   MFS1_SCR
#define MFS_ESCR  MFS1_ESCR
#define MFS_SSR   MFS1_SSR
#define MFS_TDR   MFS1_TDR
#define MFS_RDR   MFS1_RDR
#define MFS_BGR   MFS1_BGR
#if   UART_LOC == 0
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 10)) | (4 << 10); PFR5 |= 3 << 6; }
#elif UART_LOC == 1
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 10)) | (10 << 10); PFR1 |= 3 << 1; }
#elif UART_LOC == 2
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 10)) | (15 << 10); PFRF |= 3 << 0; }
#endif

#elif UART_MFS == 2
#define MFS_RX_IRQHandler MFS2_RX_IRQHandler
#define MFS_TX_IRQHandler MFS2_TX_IRQHandler
#define MFS_RX_IRQn MFS2_RX_IRQn
#define MFS_TX_IRQn MFS2_TX_IRQn
#define MFS_SMR   MFS2_SMR
#define MFS_SCR   MFS2_SCR
#define MFS_ESCR  MFS2_ESCR
#define MFS_SSR   MFS2_SSR
#define MFS_TDR   MFS2_TDR
#define MFS_RDR   MFS2_RDR
#define MFS_BGR   MFS2_BGR
#if   UART_LOC == 0
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 16)) | (4 << 16); PFR7 |= 3 << 2; }
#elif UART_LOC == 1
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 16)) | (10 << 16); PFR2 |= 3 << 4; }
#elif UART_LOC == 2
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 16)) | (15 << 16); PFR1 |= 3 << 7; }
#endif

#elif UART_MFS == 3
#define MFS_RX_IRQHandler MFS3_RX_IRQHandler
#define MFS_TX_IRQHandler MFS3_TX_IRQHandler
#define MFS_RX_IRQn MFS3_RX_IRQn
#define MFS_TX_IRQn MFS3_TX_IRQn
#define MFS_SMR   MFS3_SMR
#define MFS_SCR   MFS3_SCR
#define MFS_ESCR  MFS3_ESCR
#define MFS_SSR   MFS3_SSR
#define MFS_TDR   MFS3_TDR
#define MFS_RDR   MFS3_RDR
#define MFS_BGR   MFS3_BGR
#if   UART_LOC == 0
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 22)) | (4 << 22); PFR7 |= 3 << 5; }
#elif UART_LOC == 1
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 22)) | (10 << 22); PFR5 |= 3 << 0; }
#elif UART_LOC == 2
#define ATTACH_MFS() { EPFR07 = (EPFR07 & ~(15 << 22)) | (15 << 22); PFR4 |= 3 << 8; }
#endif

#elif UART_MFS == 4
#define MFS_RX_IRQHandler MFS4_RX_IRQHandler
#define MFS_TX_IRQHandler MFS4_TX_IRQHandler
#define MFS_RX_IRQn MFS4_RX_IRQn
#define MFS_TX_IRQn MFS4_TX_IRQn
#define MFS_SMR   MFS4_SMR
#define MFS_SCR   MFS4_SCR
#define MFS_ESCR  MFS4_ESCR
#define MFS_SSR   MFS4_SSR
#define MFS_TDR   MFS4_TDR
#define MFS_RDR   MFS4_RDR
#define MFS_BGR   MFS4_BGR
#if   UART_LOC == 0
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 4)) | (4 << 4); PFRD |= 3 << 1; }
#elif UART_LOC == 1
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 4)) | (10 << 4); PFR1 |= 3 << 10; }
#elif UART_LOC == 2
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 4)) | (15 << 4); PFR0 |= 3 << 5; }
#endif

#elif UART_MFS == 5
#define MFS_RX_IRQHandler MFS5_RX_IRQHandler
#define MFS_TX_IRQHandler MFS5_TX_IRQHandler
#define MFS_RX_IRQn MFS5_RX_IRQn
#define MFS_TX_IRQn MFS5_TX_IRQn
#define MFS_SMR   MFS5_SMR
#define MFS_SCR   MFS5_SCR
#define MFS_ESCR  MFS5_ESCR
#define MFS_SSR   MFS5_SSR
#define MFS_TDR   MFS5_TDR
#define MFS_RDR   MFS5_RDR
#define MFS_BGR   MFS5_BGR
#if   UART_LOC == 0
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 10)) | (4 << 10); PFR6 |= 3 << 0; }
#elif UART_LOC == 1
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 10)) | (10 << 10); PFR9 |= 3 << 2; }
#elif UART_LOC == 2
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 10)) | (15 << 10); PFR3 |= 3 << 6; }
#endif

#elif UART_MFS == 6
#define MFS_RX_IRQHandler MFS6_RX_IRQHandler
#define MFS_TX_IRQHandler MFS6_TX_IRQHandler
#define MFS_RX_IRQn MFS6_RX_IRQn
#define MFS_TX_IRQn MFS6_TX_IRQn
#define MFS_SMR   MFS6_SMR
#define MFS_SCR   MFS6_SCR
#define MFS_ESCR  MFS6_ESCR
#define MFS_SSR   MFS6_SSR
#define MFS_TDR   MFS6_TDR
#define MFS_RDR   MFS6_RDR
#define MFS_BGR   MFS6_BGR
#if   UART_LOC == 0
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 16)) | (4 << 16); PFR5 |= 3 << 3; }
#elif UART_LOC == 1
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 16)) | (10 << 16); PFR3 |= 3 << 2; }
#elif UART_LOC == 2
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 16)) | (15 << 16); PFRF |= 3 << 3; }
#endif

#elif UART_MFS == 7
#define MFS_RX_IRQHandler MFS7_RX_IRQHandler
#define MFS_TX_IRQHandler MFS7_TX_IRQHandler
#define MFS_RX_IRQn MFS7_RX_IRQn
#define MFS_TX_IRQn MFS7_TX_IRQn
#define MFS_SMR   MFS7_SMR
#define MFS_SCR   MFS7_SCR
#define MFS_ESCR  MFS7_ESCR
#define MFS_SSR   MFS7_SSR
#define MFS_TDR   MFS7_TDR
#define MFS_RDR   MFS7_RDR
#define MFS_BGR   MFS7_BGR
#if   UART_LOC == 0
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 22)) | (4 << 22); PFR5 |= 3 << 9; }
#elif UART_LOC == 1
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 22)) | (10 << 22); PFR4 |= 3 << 13; }
#elif UART_LOC == 2
#define ATTACH_MFS() { EPFR08 = (EPFR08 & ~(15 << 22)) | (15 << 22); PFRB |= 3 << 0; }
#endif

#endif



/* Transmission/Reception FIFO */
static volatile struct {
	uint16_t	ri, wi;
	uint8_t		buff[BUFF_SIZE];
} TxFifo, RxFifo;


void MFS_TX_IRQHandler (void)
{
	uint16_t i;


	i = TxFifo.ri;
	if (i != TxFifo.wi) {		/* There is one or more byte in the Tx buffer */
		MFS_TDR = TxFifo.buff[i++];
		TxFifo.ri = i % sizeof TxFifo.buff;
	}
	if (i == TxFifo.wi) {		/* No data in the Tx buffer */
		MFS_SCR &= ~0x08;		/* Clear TIE (disable Tx ready irq) */
	}
}


void MFS_RX_IRQHandler (void)
{
	uint8_t d;
	uint16_t i, ni;


	if (MFS_SSR & 0x38) {	/* Error occured */
		MFS_SSR = 0x80;
	}

	if (MFS_SSR & 0x04) {	/* Data arrived */
		d = MFS_RDR;	/* Get received data */
		i = RxFifo.wi;
		ni = (i + 1) % BUFF_SIZE;
		if (ni != RxFifo.ri) {	/* Store it into the Rx buffer if not full */
			RxFifo.buff[i] = d;
			RxFifo.wi = ni;
		}
	}

}



/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


int uart_test (void)	/* 0:Empty, 1:Not empty */
{
	return RxFifo.ri != RxFifo.wi;
}



uint8_t uart_getc (void)
{
	uint8_t d;
	uint16_t i;

	/* Wait while Rx FIFO is empty */
	i = RxFifo.ri;
	while (i == RxFifo.wi) ;

	d = RxFifo.buff[i++];	/* Get a byte from Rx FIFO */
	RxFifo.ri = i % sizeof RxFifo.buff;

	return d;
}



void uart_putc (uint8_t d)
{
	uint16_t i, ni;

	/* Wait for Tx FIFO ready */
	i = TxFifo.wi;
	ni = (i + 1) % sizeof TxFifo.buff;
	while (ni == TxFifo.ri) ;

	TxFifo.buff[i] = d;	/* Put a byte into Tx byffer */
	__disable_irq();
	TxFifo.wi = ni;
	MFS_SCR |= 0x08;	/* Set TIE (enable TX ready irq) */
	__enable_irq();
}



void uart_init (void)
{
	__disable_irqn(MFS_RX_IRQn);
	__disable_irqn(MFS_TX_IRQn);

	/* Initialize MFS (UART0 mode, N81)*/
	MFS_SCR = 0x80;		/* Disable MFS */
	MFS_SMR = 0x01;		/* Enable SOT output */
	MFS_ESCR = 0;
	MFS_BGR = F_PCLK / UART_BPS - 1;
	MFS_SCR = 0x13;		/* Enable MFS: Set RIE/RXE/TXE */

	/* Clear Tx/Rx buffers */
	TxFifo.ri = 0; TxFifo.wi = 0;
	RxFifo.ri = 0; RxFifo.wi = 0;

	/* Attach MFS module to I/O pads */
	ATTACH_MFS();

	/* Enable Tx/Rx/Error interrupts */
//	__set_irqn_priority(MFS_RX_IRQn, 192);
//	__set_irqn_priority(MFS_TX_IRQn, 192);
	__enable_irqn(MFS_RX_IRQn);
	__enable_irqn(MFS_TX_IRQn);
}




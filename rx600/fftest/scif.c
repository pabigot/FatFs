/*------------------------------------------------------------------------*/
/* FRK-RX62N: SCI control module                                          */
/*------------------------------------------------------------------------*/
/*
/  Copyright (C) 2012, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/

#define SCIF_CH	1	/* SCI channel (0:SCI0, 1:SCI1, 2:SCI2-A, 3:SCI3-A)	*/

#define BUFFER_SIZE 128	/* Tx/Rx FIFO size [byte]	*/

#define F_PCLK		96000000UL	/* PCLK frequency (configured by SCKCR.PCK) */

/*--------------------------------------------------*/


#include "iodefine.h"
#include "scif.h"
#include "vect.h"


#if SCIF_CH == 0	/* SCI0 macros */

#define SCIF SCI0
#define ISR_ER() void Excep_SCI0_ERI0(void)
#define ISR_RX() void Excep_SCI0_RXI0(void)
#define ISR_TX() void Excep_SCI0_TXI0(void)
#define ATTACH_SCIF() { 	\
	MSTP_SCI0 = 0;			\
	PORT2.ICR.BIT.B1 = 1;	\
}
#define IEN_SCIF() { 		\
	IPR(SCI0, ERI0) = 3; IEN(SCI0, ERI0) = 1;	\
	IPR(SCI0, RXI0) = 3; IEN(SCI0, RXI0) = 1;	\
	IPR(SCI0, TXI0) = 1; IEN(SCI0, TXI0) = 1;	\
}
#define ENABLE_RXI()	{IEN(SCI0, RXI0) = 1; IEN(SCI0, ERI0) = 1;}
#define DISABLE_RXI()	{IEN(SCI0, RXI0) = 0; IEN(SCI0, ERI0) = 0;}
#define ENABLE_TXI()	{IEN(SCI0, TXI0) = 1;}
#define DISABLE_TXI()	{IEN(SCI0, TXI0) = 0;}

#elif SCIF_CH == 1	/* SCI1 macros */
#define SCIF SCI1
#define ISR_ER() void Excep_SCI1_ERI1(void)
#define ISR_RX() void Excep_SCI1_RXI1(void)
#define ISR_TX() void Excep_SCI1_TXI1(void)
#define ATTACH_SCIF() { 	\
	MSTP_SCI1 = 0;			\
	PORT3.ICR.BIT.B0 = 1;	\
}
#define IEN_SCIF() { 		\
	IPR(SCI1, ERI1) = 3; IEN(SCI1, ERI1) = 1;	\
	IPR(SCI1, RXI1) = 3; IEN(SCI1, RXI1) = 1;	\
	IPR(SCI1, TXI1) = 1; IEN(SCI1, TXI1) = 1;	\
}
#define ENABLE_RXI()	{IEN(SCI1, RXI1) = 1; IEN(SCI1, ERI1) = 1;}
#define DISABLE_RXI()	{IEN(SCI1, RXI1) = 0; IEN(SCI1, ERI1) = 0;}
#define ENABLE_TXI()	{IEN(SCI1, TXI1) = 1;}
#define DISABLE_TXI()	{IEN(SCI1, TXI1) = 0;}

#elif SCIF_CH == 2	/* SCI2 macros */
#define SCIF SCI2
#define ISR_ER() void Excep_SCI2_ERI2(void)
#define ISR_RX() void Excep_SCI2_RXI2(void)
#define ISR_TX() void Excep_SCI2_TXI2(void)
#define ATTACH_SCIF() { 	\
	IOPORT.PFFSCI.BIT.SCI2S = 0;\
	MSTP_SCI2 = 0;			\
	PORT1.ICR.BIT.B2 = 1;	\
}
#define IEN_SCIF() { 		\
	IPR(SCI2, ERI2) = 3; IEN(SCI2, ERI2) = 1;	\
	IPR(SCI2, RXI2) = 3; IEN(SCI2, RXI2) = 1;	\
	IPR(SCI2, TXI2) = 1; IEN(SCI2, TXI2) = 1;	\
}
#define ENABLE_RXI()	{IEN(SCI2, RXI2) = 1; IEN(SCI2, ERI2) = 1;}
#define DISABLE_RXI()	{IEN(SCI2, RXI2) = 0; IEN(SCI2, ERI2) = 0;}
#define ENABLE_TXI()	{IEN(SCI2, TXI2) = 1;}
#define DISABLE_TXI()	{IEN(SCI2, TXI2) = 0;}

#elif SCIF_CH == 3	/* SCI3 macros */
#define SCIF SCI3
#define ISR_ER() void Excep_SCI3_ERI3(void)
#define ISR_RX() void Excep_SCI3_RXI3(void)
#define ISR_TX() void Excep_SCI3_TXI3(void)
#define ATTACH_SCIF() { 	\
	IOPORT.PFFSCI.BIT.SCI3S = 0;\
	MSTP_SCI3 = 0;			\
	PORT1.ICR.BIT.B6 = 1;	\
}
#define IEN_SCIF() { 		\
	IPR(SCI3, ERI3) = 3; IEN(SCI3, ERI3) = 1;	\
	IPR(SCI3, RXI3) = 3; IEN(SCI3, RXI3) = 1;	\
	IPR(SCI3, TXI3) = 1; IEN(SCI3, TXI3) = 1;	\
}
#define ENABLE_RXI()	{IEN(SCI3, RXI3) = 1; IEN(SCI3, ERI3) = 1;}
#define DISABLE_RXI()	{IEN(SCI3, RXI3) = 0; IEN(SCI3, ERI3) = 0;}
#define ENABLE_TXI()	{IEN(SCI3, TXI3) = 1;}
#define DISABLE_TXI()	{IEN(SCI3, TXI3) = 0;}

#endif

#define IODLY() {ICU.IR[0].BYTE; ICU.IR[0].BYTE; ICU.IR[0].BYTE;}


extern void delay_ms(unsigned int);	/* Defined in main.c */


/* Tx/Rx FIFO  */
static volatile struct {
	uint16_t	ri, wi, ct, run;
	uint8_t		buff[BUFFER_SIZE];
} TxFifo, RxFifo;



/*---------------------------------------*/
/* Initialize SCI                        */
/*---------------------------------------*/

void scif_init (
	uint32_t bps
)
{
	ATTACH_SCIF();

	SCIF.SCR.BYTE = 0;
	SCIF.SMR.BYTE = 0x00;	/* CM=0, CHR=0, PE=0, PM=0, STOP=0, MP=0, CKS=0 */
	SCIF.BRR = F_PCLK / 32 / bps - 1;
	delay_ms(2);
	SCIF.SCR.BYTE = 0xF0;		/* Start SCIF (TIE=1,RIE=1,TE=1,RE=0) */

	/* Clear Tx/Rx Buffer */
	TxFifo.ri = 0; TxFifo.wi = 0; TxFifo.ct = 0; TxFifo.run = 0;
	RxFifo.ri = 0; RxFifo.wi = 0; RxFifo.ct = 0;

	IEN_SCIF();
}



/*---------------------------------------*/
/* Get number of bytes in the Rx Buffer  */
/*---------------------------------------*/

int scif_test (void)
{
	return RxFifo.ct;
}



/*---------------------------------------*/
/* Get a byte from Rx buffer             */
/*---------------------------------------*/

uint8_t scif_getc (void)
{
	uint8_t d;
	uint16_t i;


	while (!RxFifo.ct) ;		/* Wait for Rx data available */

	DISABLE_RXI();
	IODLY();

	i = RxFifo.ri;
	d = RxFifo.buff[i++];		/* Get a byte from Rx buffer */
	RxFifo.ri = i % BUFFER_SIZE;
	RxFifo.ct--;

	ENABLE_RXI();

	return d;
}



/*---------------------------------------*/
/* Put a byte into Tx buffer             */
/*---------------------------------------*/

void scif_putc (
	uint8_t dat
)
{
	uint16_t i;


	while (TxFifo.ct >= BUFFER_SIZE) ;	/* Wait for buffer ready */

	DISABLE_TXI();
	IODLY();

	if (TxFifo.run) {
		i = TxFifo.wi;			/* Store the data into the Tx buffer */
		TxFifo.buff[i++] = dat;
		TxFifo.wi = i % BUFFER_SIZE;
		TxFifo.ct++;
	} else {
		TxFifo.run = 1;
		SCIF.TDR = dat;
	}

	ENABLE_TXI();
}



/*---------------------------------------*/
/* RDR Ready ISR                         */
/*---------------------------------------*/


ISR_RX()
{
	uint16_t i;
	uint8_t d;


	d = SCIF.RDR;

	if (RxFifo.ct < BUFFER_SIZE) {	/* Store the byte if buffer is not full */
		i = RxFifo.wi;
		RxFifo.buff[i++] = d;
		RxFifo.wi = i % BUFFER_SIZE;
		RxFifo.ct++;
	}
}



/*---------------------------------------*/
/* Receive Error ISR                     */
/*---------------------------------------*/


ISR_ER()
{
	SCIF.RDR;
	SCIF.SSR.BYTE = 0x84;
}



/*---------------------------------------*/
/* TDR ready ISR                         */
/*---------------------------------------*/

ISR_TX()
{
	uint16_t i;


	if (TxFifo.ct) {
		i = TxFifo.ri;
		SCIF.TDR = TxFifo.buff[i++];
		TxFifo.ri = i % BUFFER_SIZE;
		TxFifo.ct--;
	} else {
		TxFifo.run = 0;
	}
}



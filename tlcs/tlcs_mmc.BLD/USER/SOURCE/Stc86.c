/************************************************************************
*		Standard startup program for TLCS-870/C 		*
*			 (Model : TMP86CH06N/TMP86CH06U)		*
*			     Revision: 1.0 				*
*-----------------------------------------------------------------------*
*	Copyright(C) 1998 TOSHIBA CORPORATION All rights reserved	*
************************************************************************/

#include <stdlib.h>
#include "integer.h"
#include "monitor.h"
#include "io86fm29.h"
#include "io86fm29.c"

/*=======================================================================
  [ User definition part ]
    This part is necessary.
  =======================================================================*/
/* This part is necessary. */
#define	_BaseSP		0x063f		/* Stack area initial address */
#define	_BaseIntTbl	0xffe0

/* When you need the RAM clear, this part is necessary. */
#define	RAM_TOP		0x0040		/* Start address of RAM */
#define RAM_SIZE	0x0600		/* Size of RAM */

/* This part is necessary. */
void	main(void);			/* Prototype of main function */
void	InitIo(void);		/* Prototype of IO initialization function */

/*=======================================================================
  [ Dummy section definition ]
    When using variables with initial value, this part is necessary.
  =======================================================================*/
extern  char	__tiny _TDataAddr[];	/* for t_data initialize, defined in link command file  */
extern  char	__near _TDataOrg[];	/* for t_data initialize, defined in link command file  */
extern  char	__tiny _TDataSize;	/* for t_data initialize, defined in link command file  */
extern  char	_NDataAddr[];		/* for n_data initialize, defined in link command file  */
extern  char	_NDataOrg[];		/* for n_data initialize, defined in link command file  */
extern  short	_NDataSize;			/* for n_data initialize, defined in link command file  */

/*=======================================================================
  [ Startup ]
    This part is necessary.
  =======================================================================*/
void	startup(void){
	__DI();					/* [NECESSARY] Disable interrupt */
	__SP	= _BaseSP;		/* [NECESSARY] setup SP */

/*---[ RAM clear : Using as the need arises. ]---*/
	__IX=RAM_TOP;			/* IX : RAM top address */
	__IY=RAM_SIZE-1;		/* IY : Clear RAM size  */
__asm("ram_clear:");
	__asm("	LD	(IX),0");		/* RAM Zero clear */
	__asm("	INC	IX");			/* Address increment */
	__asm("	DEC	IY");			/* Counter decrement */
	__asm("	J	F,ram_clear");		/* Counter > 0 goto RAM_CLEAR */

/*---[ Initialize of t_data section : Using as the need arises. ]---*/
	__asm("	LD	IX,__TDataOrg");
	__asm("	LD	IY,__TDataAddr");
	__asm("	LD	A,__TDataSize");
	__asm("	DEC	A");
	__asm("	J	T,non_t_data");
__asm("move_t_data:") ;
	__asm("	LD	C,(IX)");
	__asm("	LD	(IY),C");
	__asm("	INC	IX");
	__asm("	INC	IY");
	__asm("	DEC	A");
	__asm("	J	F,move_t_data");
__asm("non_t_data:") ;

/*---[ Initialize of n_data section : Using as the need arises. ]---*/
	__asm("	LD	IX,__NDataOrg");
	__asm("	LD	IY,__NDataAddr") ;
	__asm("	LD	WA,__NDataSize");
	__asm("	DEC	WA");
	__asm("	J	T,non_n_data");
__asm("move_n_data:") ;
	__asm("	LD	C,(IX)");
	__asm("	LD	(IY),C");
	__asm("	INC	IX");
	__asm("	INC	IY");
	__asm("	DEC	WA");
	__asm("	J	F,move_n_data");
__asm("non_n_data:");


	/* Initialize Peripherals */
	SYSCR2 = 0xC0;			/* Enter NORMAL2 mode */
	TBTCR = 0x18;			/* Clock source = slow clock, Enable TBT (1Hz) */
	TTREG4 = 41;			/* Enable TC4 in 100Hz timer */
	TC4CR = 0x08;

	EEPCR = 0xC1;			/* Enable automatic memory power control */

	P1DR = 0xFD;			/* P1 = I TD RD I I I L I */
	P6CR = 0xFF;			/* P6 = output */

	IL = 0;					/* Interrupt latch clear */
	EIR = 0x0840;			/* Enable TBT, TC4 */

	uart_init();			/* Start UART */

	__EI();					/* Enable interrupt (necessary) */

	/* Start application code */
	main();

	for(;;);
}


/*========================================================
  [ Dummy function for interrupt ]
    When you use all interrupt, you do not need this part.
  ========================================================*/
void __interrupt Int_dummy(void) {}	/* Interrupt function */
//void __interrupt_n Int_n_dummy(void) {}	/* NMI fuction */

void __interrupt Int_TBT(void);
void __interrupt Int_TC4(void);
void __interrupt Int_UartRx(void);


/*============================================
  [ Define interrupt table ]
    This part must be rewrite.
  ============================================*/
#pragma section const INT_VECTOR _BaseIntTbl
void * const IntTbl[] = {
	Int_dummy,		/* INT5/INTTC5: */
	Int_dummy,		/* INT3/INTTC3: */
	Int_dummy,		/* INTADC: ADC EOC */
	Int_dummy,		/* INTTC6: Timer 6 */
	Int_TC4,		/* INTTC4: Timer 4 */
	Int_dummy,		/* INTTXD: UART transmit */
	Int_UartRx,		/* INTRXD/INTSIO: UART receive, SIO */
	Int_dummy,		/* INTTC1: 18bit timer 1 */
	Int_dummy,		/* INT2: External interrupt 2 */
	Int_TBT,		/* INTTBT: Time Base Timer */
	Int_dummy,		/* INT1: External interrupt 1 */
	Int_dummy,		/* INT0: External interrupt 0 */
	startup,		/* INTWDT: Watchdog Timer */
	startup,		/* INTATRAP: Address Trap */
	startup,		/* INTSWI/INTUNDEF: Software interrupt / Undefined Instruction */
	startup			/* Reset */
};



/*============================================
  [ Standard library function: exit() ]
    This part must be rewrite.
  ============================================*/
void	exit(int status){
	for(;;);
}


/*============================================
  [ Standard library function: abort() ]
    This part must be rewrite.
  ============================================*/
void	abort(void){
	__asm("	;j	__startup");
}



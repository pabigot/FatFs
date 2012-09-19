/**** TMP86FM29 Register Definitions *****/

#include "integer.h"

extern BYTE	__io(0x01) P1DR;		/* Port1 data */
extern BYTE	__io(0x02) P2DR;		/* Port2 data */
extern BYTE	__io(0x03) P3DR;		/* Port3 data */
extern BYTE	__io(0x04) P3OUTCR;		/* Port3 direction */
extern BYTE	__io(0x05) P5DR;		/* Port5 data */
extern BYTE	__io(0x06) P6DR;		/* Port6 data */
extern BYTE	__io(0x07) P7DR;		/* Port7 data */

extern BYTE	__io(0x08) P1PRD;		/* Port1 pin */
extern BYTE	__io(0x09) P2PRD;		/* Port2 pin */
extern BYTE	__io(0x0A) P3PRD;		/* Port3 pin */
extern BYTE	__io(0x0B) P5PRD;		/* Port5 pin */
extern BYTE	__io(0x0C) P6CR;		/* Port6 direction */
extern BYTE	__io(0x0D) P7PRD;		/* Port7 pin */
extern BYTE	__io(0x0E) ADCCR1;		/* ADC control 1 */
extern BYTE	__io(0x0F) ADCCR2;		/* ADC control 2 */

extern BYTE	__io(0x10) TREGAL;		/*  */
extern BYTE	__io(0x11) TREG1AM;		/*  */
extern BYTE	__io(0x12) TREG1AH;		/*  */
extern BYTE	__io(0x13) TREG1B;		/*  */
extern BYTE	__io(0x14) TC1CR1;		/*  */
extern BYTE	__io(0x15) TC1CR2;		/*  */
extern BYTE	__io(0x16) TC1SR;		/*  */

extern BYTE	__io(0x18) TC3CR;		/*  */
extern BYTE	__io(0x19) TC4CR;		/*  */
extern BYTE	__io(0x1A) TC5CR;		/*  */
extern BYTE	__io(0x1B) TC6CR;		/*  */
extern BYTE	__io(0x1C) TTREG3;		/*  */
extern BYTE	__io(0x1D) TTREG4;		/*  */
extern BYTE	__io(0x1E) TTREG5;		/*  */
extern BYTE	__io(0x1F) TTREG6;		/*  */

extern BYTE	__io(0x20) ADCDR1;		/*  */
extern BYTE	__io(0x21) ADCDR2;		/*  */
extern BYTE	__io(0x25) UARTCSR1;	/*  */
extern BYTE	__io(0x26) UARTCR2;		/*  */

extern BYTE	__io(0x28) LCDCR;		/*  */
extern BYTE	__io(0x29) P1LCR;		/*  */
extern BYTE	__io(0x2A) P5LCR;		/*  */
extern BYTE	__io(0x2B) P7LCR;		/*  */
extern BYTE	__io(0x2C) PWREG3;		/*  */
extern BYTE	__io(0x2D) PWREG4;		/*  */
extern BYTE	__io(0x2E) PWREG5;		/*  */
extern BYTE	__io(0x2F) PWREG6;		/*  */

extern BYTE	__io(0x34) WDTCR1;		/*  */
extern BYTE	__io(0x35) WDTCR2;		/*  */
extern BYTE	__io(0x36) TBTCR;		/*  */
extern BYTE	__io(0x37) EINTCR;		/*  */

extern BYTE	__io(0x38) SYSCR1;		/*  */
extern BYTE	__io(0x39) SYSCR2;		/*  */
extern WORD	__io(0x3A) EIR;			/*  */

extern WORD	__io(0x3C) IL;			/*  */
extern BYTE	__io(0x3E) INTSEL;	 	/*  */

#define LCDREG ((BYTE *)0x0F80)

#define SIOBR (*(volatile BYTE *)0x0F90)
#define SIOCR1 (*(volatile BYTE *)0x0F98)
#define SIOCR2 (*(volatile BYTE *)0x0F99)
#define SIOSO (*(volatile BYTE *)0x0F99)
#define STOPCR (*(volatile BYTE *)0x0F9A)
#define RDBUF (*(volatile BYTE *)0x0F9B)
#define TDBUF (*(volatile BYTE *)0x0F9B)
#define EEPCR (*(volatile BYTE *)0x0FE0)
#define EEPSR (*(volatile BYTE *)0x0FE1)



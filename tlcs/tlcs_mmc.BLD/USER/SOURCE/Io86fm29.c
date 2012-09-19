BYTE	__io(0x01) P1DR;		/* Port1 data */
BYTE	__io(0x02) P2DR;		/* Port2 data */
BYTE	__io(0x03) P3DR;		/* Port3 data */
BYTE	__io(0x04) P3OUTCR;		/* Port3 direction */
BYTE	__io(0x05) P5DR;		/* Port5 data */
BYTE	__io(0x06) P6DR;		/* Port6 data */
BYTE	__io(0x07) P7DR;		/* Port7 data */

BYTE	__io(0x08) P1PRD;		/* Port1 pin */
BYTE	__io(0x09) P2PRD;		/* Port2 pin */
BYTE	__io(0x0A) P3PRD;		/* Port3 pin */
BYTE	__io(0x0B) P5PRD;		/* Port5 pin */
BYTE	__io(0x0C) P6CR;		/* Port6 direction */
BYTE	__io(0x0D) P7PRD;		/* Port7 pin */
BYTE	__io(0x0E) ADCCR1;		/* ADC control 1 */
BYTE	__io(0x0F) ADCCR2;		/* ADC control 2 */

BYTE	__io(0x10) TREGAL;		/*  */
BYTE	__io(0x11) TREG1AM;		/*  */
BYTE	__io(0x12) TREG1AH;		/*  */
BYTE	__io(0x13) TREG1B;		/*  */
BYTE	__io(0x14) TC1CR1;		/*  */
BYTE	__io(0x15) TC1CR2;		/*  */
BYTE	__io(0x16) TC1SR;		/*  */

BYTE	__io(0x18) TC3CR;		/*  */
BYTE	__io(0x19) TC4CR;		/*  */
BYTE	__io(0x1A) TC5CR;		/*  */
BYTE	__io(0x1B) TC6CR;		/*  */
BYTE	__io(0x1C) TTREG3;		/*  */
BYTE	__io(0x1D) TTREG4;		/*  */
BYTE	__io(0x1E) TTREG5;		/*  */
BYTE	__io(0x1F) TTREG6;		/*  */

BYTE	__io(0x20) ADCDR1;		/*  */
BYTE	__io(0x21) ADCDR2;		/*  */
BYTE	__io(0x25) UARTCSR1;	/*  */
BYTE	__io(0x26) UARTCR2;		/*  */

BYTE	__io(0x28) LCDCR;		/*  */
BYTE	__io(0x29) P1LCR;		/*  */
BYTE	__io(0x2A) P5LCR;		/*  */
BYTE	__io(0x2B) P7LCR;		/*  */
BYTE	__io(0x2C) PWREG3;		/*  */
BYTE	__io(0x2D) PWREG4;		/*  */
BYTE	__io(0x2E) PWREG5;		/*  */
BYTE	__io(0x2F) PWREG6;		/*  */

BYTE	__io(0x34) WDTCR1;		/*  */
BYTE	__io(0x35) WDTCR2;		/*  */
BYTE	__io(0x36) TBTCR;		/*  */
BYTE	__io(0x37) EINTCR;		/*  */

BYTE	__io(0x38) SYSCR1;		/*  */
BYTE	__io(0x39) SYSCR2;		/*  */
WORD	__io(0x3A) EIR;			/*  */
WORD	__io(0x3C) IL;			/*  */
BYTE	__io(0x3E) INTSEL;	 	/*  */

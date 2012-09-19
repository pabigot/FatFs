-------- PROJECT GENERATOR --------
PROJECT NAME :	cq_rx_ff
PROJECT DIRECTORY :	C:\user\Renesas\cq_rx\cq_rx_ff\cq_rx_ff
CPU SERIES :	RX600
CPU TYPE :	RX62N
TOOLCHAIN NAME :	Renesas RX Standard Toolchain
TOOLCHAIN VERSION :	1.0.1.0
GENERATION FILES :
    C:\user\Renesas\cq_rx\cq_rx_ff\cq_rx_ff\dbsct.c
        Setting of B,R Section
    C:\user\Renesas\cq_rx\cq_rx_ff\cq_rx_ff\typedefine.h
        Aliases of Integer Type
    C:\user\Renesas\cq_rx\cq_rx_ff\cq_rx_ff\iodefine.h
        Definition of I/O Register
    C:\user\Renesas\cq_rx\cq_rx_ff\cq_rx_ff\intprg.c
        Interrupt Program
    C:\user\Renesas\cq_rx\cq_rx_ff\cq_rx_ff\vecttbl.c
        Initialize of Vector Table
    C:\user\Renesas\cq_rx\cq_rx_ff\cq_rx_ff\vect.h
        Definition of Vector
    C:\user\Renesas\cq_rx\cq_rx_ff\cq_rx_ff\resetprg.c
        Reset Program
    C:\user\Renesas\cq_rx\cq_rx_ff\cq_rx_ff\cq_rx_ff.c
        Main Program
    C:\user\Renesas\cq_rx\cq_rx_ff\cq_rx_ff\stacksct.h
        Setting of Stack area
START ADDRESS OF SECTION :
 H'1000	B_1,R_1,B_2,R_2,B,R,SU,SI
 H'FFFF8000	PResetPRG
 H'FFFF8100	C_1,C_2,C,C$*,D*,P,PIntPRG,W*
 H'FFFFFFD0	FIXEDVECT

* When the user program is executed,
* the interrupt mask has been masked.
* 
* Program start 0xFFFF8000.
* RAM start 0x1000.

DATE & TIME : 2011/01/25 22:09:20

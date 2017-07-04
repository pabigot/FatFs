-------- PROJECT GENERATOR --------
PROJECT NAME :	ff_rx220
PROJECT DIRECTORY :	C:\user\Prj_Renesas\ff_rx220\ff_rx220
CPU SERIES :	RX200
CPU TYPE :	RX220
TOOLCHAIN NAME :	Renesas RX Standard Toolchain
TOOLCHAIN VERSION :	1.2.1.0
GENERATION FILES :
    C:\user\Prj_Renesas\ff_rx220\ff_rx220\dbsct.c
        Setting of B,R Section
    C:\user\Prj_Renesas\ff_rx220\ff_rx220\typedefine.h
        Aliases of Integer Type
    C:\user\Prj_Renesas\ff_rx220\ff_rx220\iodefine.h
        Definition of I/O Register
    C:\user\Prj_Renesas\ff_rx220\ff_rx220\intprg.c
        Interrupt Program
    C:\user\Prj_Renesas\ff_rx220\ff_rx220\vecttbl.c
        Initialize of Vector Table
    C:\user\Prj_Renesas\ff_rx220\ff_rx220\vect.h
        Definition of Vector
    C:\user\Prj_Renesas\ff_rx220\ff_rx220\resetprg.c
        Reset Program
    C:\user\Prj_Renesas\ff_rx220\ff_rx220\ff_rx220.c
        Main Program
    C:\user\Prj_Renesas\ff_rx220\ff_rx220\stacksct.h
        Setting of Stack area
START ADDRESS OF SECTION :
 H'4	B_1,R_1,B_2,R_2,B,R,SU,SI
 H'FFFF8000	PResetPRG
 H'FFFF8100	C_1,C_2,C,C$*,D_1,D_2,D,P,PIntPRG,W*,L
 H'FFFFFFD0	FIXEDVECT

* When the user program is executed,
* the interrupt mask has been masked.
* 
* Program start 0xFFFF8000.
* RAM start 0x4.

DATE & TIME : 2016/11/08 22:36:16

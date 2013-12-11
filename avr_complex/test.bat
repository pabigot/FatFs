avr-gcc -mmcu=atmega64 -Os -mcall-prologues -gdwarf-2 -c ff.c
@if ERRORLEVEL 1 goto exit
avr-objdump -S ff.o
avr-size ff.o
del ff.o
:exit

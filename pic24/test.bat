pic30-gcc -mcpu=24FJ64GA002 -x c -c ff.c -o ff.o -g -Wall -pedantic -Os
@if ERRORLEVEL 1 goto exit
pic30-objdump -s ff.o
del ff.o
:exit

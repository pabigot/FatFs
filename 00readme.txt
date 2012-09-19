FatFs Module Sample Projects                             (C)ChaN, 2009


DIRECTORIES

  <avr>     ATMEL AVR (ATmega64,8-bit,RISC) with MMC, CFC and ATA.
  <lpc2k>   NXP LPC2000 (LPC2368,32-bit,RISC) with MMC.
  <h8>      Renesas H8/300H (HD64F3694,16-bit,CISC) with MMC.
  <pic>     Microchip PIC (PIC24FJ64GA002,16-bit,RISC) with MMC.
  <tlcs>    TOSHIBA TLCS-870/C (TMP86FM29,8-bit,CISC) with MMC.
  <v850>    NEC V850ES (UPD70F3716,32-bit,RISC) with MMC.
  <win32>   Windows (VC++ 6.0)

  These are sample projects for function/compatibility test of FatFs module
  with low level disk I/O codes. The disk I/O modules will able to be used
  for any other file system module as well. You will able to find various
  implementations on the web other than these samples, such as SH2, LPC2k,
  STR7, MSP430, PIC and Z8, at least, so far as I know.



AGREEMENTS

  These sample projects for FatFs module are free software and there is no warranty.
  You can use, modify and redistribute it for personal, non-profit or commercial
  use without any restriction under your responsibility.



REVISION HISTORY

  Apr 29, 2006  First release.
  Aug 19, 2006  MMC module: Fixed a bug that disk_initialize() never time-out
                when card does not go ready.
  Oct 12, 2006  CF module: Fixed a bug that disk_initialize() can fail at 3.3V.
  Oct 22, 2006  Added a sample project for V850ES.
  Feb 04, 2007  All modules: Modified for FatFs module R0.04.
                MMC module: Fixed a bug that disk_ioctl() returns incorrect disk size.
  Apr 03, 2007  All modules: Modified for FatFs module R0.04a.
                MMC module: Supported high capacity SD memory cards.
  May 05, 2007  MMC modules: Fixed a bug that GET_SECTOR_COUNT via disk_ioctl() fails on MMC.
  Aug 26, 2007  Added some ioctl sub-functions.
  Oct 13, 2007  MMC modules: Fixed send_cmd() sends incorrect command packet.
  Dec 12, 2007  Added a sample project for Microchip PIC.
  Feb 03, 2008  All modules: Modified for FatFs module R0.05a.
  Apr 01, 2008  Modified main() for FatFs module R0.06.
  Oct 18, 2008  Added a sample project for NXP LPC2000.
  Apr 01, 2009  Modified for FatFs module R0.07.

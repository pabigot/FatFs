/*------------------------------------------------------------------------/
/  LPC1700 RTC control module
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2011, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/

#include "LPC1700.h"
#include "rtc.h"


int rtc_initialize (void)
{
	/* Enable PCLK to the RTC */
	__set_PCONP(PCRTC, 1);

	/* Start RTC with external XTAL */
	CCR = 0x11;

	return 1;
}



int rtc_gettime (RTC *rtc)
{
	DWORD d, t;


	do {
		t = CTIME0;
		d = CTIME1;
	} while (t != CTIME0 || d != CTIME1);

	if (RTC_AUX & _BV(4)) {
		rtc->sec = 0;
		rtc->min = 0;
		rtc->hour = 0;
		rtc->wday = 0;
		rtc->mday = 1;
		rtc->month = 1;
		rtc->year = 2011;
		return 0;
	}

	rtc->sec = t & 63;
	rtc->min = (t >> 8) & 63;
	rtc->hour = (t >> 16) & 31;
	rtc->wday = (t >> 24) & 7;
	rtc->mday = d & 31;
	rtc->month = (d >> 8) & 15;
	rtc->year = (d >> 16) & 4095;
	return 1;
}




int rtc_settime (const RTC *rtc)
{
	CCR = 0x12;		/* Stop RTC */

	/* Update RTC registers */
	SEC = rtc->sec;
	MIN = rtc->min;
	HOUR = rtc->hour;
	DOW = rtc->wday;
	DOM = rtc->mday;
	MONTH = rtc->month;
	YEAR = rtc->year;

	RTC_AUX = _BV(4);	/* Clear power fail flag */
	CCR = 0x11;			/* Restart RTC, Disable calibration feature */

	return 1;
}



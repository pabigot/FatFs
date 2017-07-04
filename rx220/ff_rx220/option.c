/* MDE register (Single Chip Mode) */
#pragma address _MDEreg=0xFFFFFF80
const unsigned long _MDEreg =
#ifdef __BIG
0xFFFFFFF8;
#else
0xFFFFFFFF;
#endif

/* OFS1 register */
#pragma address _OFS1reg=0xFFFFFF88
const unsigned long _OFS1reg = 0xFFFFFFF9;

/* OFS0 register */
#pragma address _OFS0reg=0xFFFFFF8C
const unsigned long _OFS0reg = 0xFFFFFFFF;


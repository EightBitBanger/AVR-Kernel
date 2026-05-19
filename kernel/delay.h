#ifndef _DELAY_FREQUENCY__
#define _DELAY_FREQUENCY__

#ifdef KERNEL_PLATFORM_AVR
  
  #define F_CPU 20000000UL
  #include <util/delay.h>
  
#endif


#endif

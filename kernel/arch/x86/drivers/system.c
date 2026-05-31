#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef KERNEL_PLATFORM_X86
  #include <kernel/arch/x86/io.h>
#endif

#ifdef KERNEL_PLATFORM_AVR
  #include <kernel/arch/avr/io.h>
#endif


void system_restart(void) {
    
    outb(0xCF9, 0x06);
}

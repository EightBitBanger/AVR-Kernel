#ifndef _DRIVER_INITIATION__
#define _DRIVER_INITIATION__

//
// Define baked drivers to be compiled into
// the kernel statically and available at runtime.

#include <avr/io.h>

#include <drivers/display/LiquidCrystalDisplayController/main.h>
#include <drivers/keyboard/ps2/main.h>

void InitBakedDrivers(void);

#endif

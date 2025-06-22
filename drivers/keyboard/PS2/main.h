#ifndef __PS2_KEYBOARD_DRIVER_
#define __PS2_KEYBOARD_DRIVER_

#include <avr/io.h>

#include <kernel/bus/bus.h>
#include <kernel/device/device.h>

uint8_t kbDecodeScanCode(uint8_t scancode_low, uint8_t scancode_high);

void kbPrintInput(void);

struct PS2DeviceDriver {
    
    struct Device device;
    
    uint8_t is_linked;
    
    void(*read)(uint32_t address, uint8_t* buffer);
    void(*write)(uint32_t address, uint8_t buffer);
    
};

void initiatePS2Driver(void);

#endif

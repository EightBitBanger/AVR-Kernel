#ifndef __NETWEORK_INTERFACE_DRIVER_
#define __NETWEORK_INTERFACE_DRIVER_

#include <avr/io.h>

#include <kernel/bus/bus.h>
#include <kernel/device/device.h>

enum BaudRates {
    
    BAUD_900    = 0x00,
    BAUD_1200   = 0x01,
    BAUD_2400   = 0x02,
    BAUD_4800   = 0x03,
    BAUD_9600   = 0x04,
    BAUD_28K    = 0x05,
    BAUD_33K    = 0x06,
    BAUD_56K    = 0x07,
    BAUD_76K    = 0x08,
    BAUD_125K   = 0x09,
    
};

struct NetworkDeviceDriver {
    
    struct Device device;
    
    struct Bus interface;
    
    uint8_t is_linked;
    
    void(*read)(uint32_t address, uint8_t* buffer);
    void(*write)(uint32_t address, uint8_t buffer);
    
};

void initiateNetworkDriver(void);

#endif

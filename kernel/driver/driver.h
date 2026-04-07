#ifndef DRIVER_SYSTEM_H
#define DRIVER_SYSTEM_H

#include <stdint.h>

struct Driver {
    //struct Device device;
    uint8_t is_linked;
    
    void (*read)(uint32_t address, uint8_t* buffer);
    void (*write)(uint32_t address, uint8_t buffer);
};

#endif

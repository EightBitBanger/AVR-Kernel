#ifndef DEVICE_BLOCK_H
#define DEVICE_BLOCK_H

#include <kernel/bus/bus.h>

#include <stdint.h>

struct Device {
    char device_name[16];
    uint8_t device_id;
    uint8_t device_class;
    
    uint8_t  hardware_slot;
    uint32_t hardware_address;
    
    uint32_t block_address;
    struct Bus bus;
};

#endif

#include <kernel/fs/fs.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

uint8_t fs_readb(uint32_t address) {
    extern uint32_t fs_device_address;
    extern struct Bus fs_bus;
    uint8_t byte;
    mmio_readb(&fs_bus, fs_device_address + address, &byte);
    return byte;
}

void fs_writeb(uint32_t address, uint8_t byte) {
    extern uint32_t fs_device_address;
    extern struct Bus fs_bus;
    uint8_t check_byte;
    mmio_readb(&fs_bus, fs_device_address + address, &check_byte);
    if (byte == check_byte) return;
    mmio_writeb(&fs_bus, fs_device_address + address, &byte);
    
    uint16_t retry = 1000;
    while (check_byte != byte && retry != 0) {
        _delay_us(100);
        mmio_readb(&fs_bus, fs_device_address + address, &check_byte);
        retry--;
    }
}

void fs_mem_read(uint32_t address, void* destination, uint32_t size) {
    uint8_t* bytes = (uint8_t*)destination;
    for (uint32_t index = 0; index < size; index++) 
        bytes[index] = fs_readb(address + index);
}

void fs_mem_write(uint32_t address, const void* source, uint32_t size) {
    const uint8_t* bytes = (const uint8_t*)source;
    for (uint32_t index = 0; index < size; index++) 
        fs_writeb(address + index, bytes[index]);
}

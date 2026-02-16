#ifndef __BUS_INTERFACE_
#define __BUS_INTERFACE_

#include <stdint.h>

struct Bus {
    // Wait states
    uint16_t read_waitstate;
    uint16_t write_waitstate;
    
    // Physical interface type
    uint8_t bus_type;
};

void mmio_read_byte(struct Bus* bus, uint32_t address, uint8_t* buffer);
void mmio_write_byte(struct Bus* bus, uint32_t address, uint8_t byte);

void mmio_read_cache(struct Bus* bus, uint32_t address, uint8_t* buffer);
void mmio_write_cache(struct Bus* bus, uint32_t address, uint8_t byte);

// EEPROM specific write - 10 milliseconds wait state for EEPROM writes
void mmio_write_byte_eeprom(struct Bus* bus, uint32_t address, uint8_t byte);

#endif

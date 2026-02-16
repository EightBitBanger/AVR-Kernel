#include <avr/io.h>

#include <kernel/delay.h>
#include <kernel/kernel.h>

#include <kernel/bus/bus.h>


#ifdef BOARD_AVR_RETROBOARD
 #include <kernel/arch/avr/io.h>
#endif


#define CACHE_SIZE   32

uint8_t cache[CACHE_SIZE];

uint8_t dirty_bits[CACHE_SIZE];

uint32_t cache_begin = 0;
uint32_t cache_end   = 0;



void mmio_read_byte(struct Bus* bus, uint32_t address, uint8_t* buffer) {
	_BUS_UPPER_OUT__  = (address >> 16) & 0xff;
	_BUS_MIDDLE_OUT__ = (address >> 8) & 0xff;
	_BUS_LOWER_OUT__  =  address & 0xff;
	
    // Latch in preparation of a read cycle
	_CONTROL_OUT__ = _CONTROL_OPEN_LATCH__;
	_CONTROL_OUT__ = _CONTROL_READ_LATCH__;
	
	// Set data direction
	_BUS_LOWER_DIR__ = 0x00;
	
	// Begin the read strobe
	_CONTROL_OUT__ = _CONTROL_READ_CYCLE__;
	
	// Wait state
	uint16_t wait = bus->read_waitstate;
    while (wait--) __asm__ volatile("nop");
	
	// Read the data
	*buffer = _BUS_LOWER_IN__;
	
	// End cycle (reset the select logic)
	_CONTROL_OUT__ = _CONTROL_OPEN_LATCH__;
	_BUS_UPPER_OUT__  = 0x00;
	_CONTROL_OUT__ = _CONTROL_CLOSED_LATCH__;
	_BUS_LOWER_DIR__ = 0xff;
}

void mmio_write_byte(struct Bus* bus, uint32_t address, uint8_t byte) {
	_BUS_UPPER_OUT__  = (address >> 16) & 0xff;
	_BUS_MIDDLE_OUT__ = (address >> 8) & 0xff;
	_BUS_LOWER_OUT__  =  address & 0xff;
	
	// Latch in preparation of a write cycle
	_CONTROL_OUT__ = _CONTROL_OPEN_LATCH__;
	_CONTROL_OUT__ = _CONTROL_WRITE_LATCH__;
	
	// Cast the data byte
	_BUS_LOWER_OUT__ = byte;
	
	// Begin the write strobe
	_CONTROL_OUT__ = _CONTROL_WRITE_CYCLE__;
	
	// Wait state
	uint16_t wait = bus->write_waitstate;
    while (wait--) __asm__ volatile("nop");
	
	// End cycle (reset the select logic)
	_CONTROL_OUT__ = _CONTROL_OPEN_LATCH__;
	_BUS_UPPER_OUT__  = 0x00;
	_CONTROL_OUT__ = _CONTROL_CLOSED_LATCH__;
}

void mmio_write_byte_eeprom(struct Bus* bus, uint32_t address, uint8_t byte) {
    mmio_write_byte(bus, address, byte);
    _delay_ms(10);
}


void mmio_flush_cache(struct Bus* bus, uint32_t address) {
    // Write back only modified cache bytes
    for (uint32_t i = 0; i < CACHE_SIZE; i++) {
        if (dirty_bits[i]) {  // Only write if modified
            mmio_write_byte(bus, cache_begin + i, cache[i]);
            dirty_bits[i] = 0; // Clear dirty bit after writing
        }
    }
    
    // Fetch new data
    for (uint32_t i = 0; i < CACHE_SIZE; i++) {
        mmio_read_byte(bus, address + i, &cache[i]);
        dirty_bits[i] = 0; // Reset dirty state for new data
    }
    
    cache_begin = address;
    cache_end = address + CACHE_SIZE;
}

void mmio_read_cache(struct Bus* bus, uint32_t address, uint8_t* buffer) {
	if (address >= cache_begin && address < cache_end) {
        *buffer = cache[address - cache_begin];
        return;
    }
    
    mmio_flush_cache(bus, address);
    *buffer = cache[0];
}


void mmio_write_cache(struct Bus* bus, uint32_t address, uint8_t byte) {
	if (address >= cache_begin && address < cache_end) {
        uint32_t offset = address - cache_begin;
        cache[offset] = byte;
        dirty_bits[offset] = 1;  // Mark as modified
        return;
    }
    
    mmio_flush_cache(bus, address);
    cache[0] = byte;
    dirty_bits[0] = 1;
}

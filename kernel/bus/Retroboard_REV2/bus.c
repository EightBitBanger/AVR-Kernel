#include <avr/io.h>

#include <kernel/delay.h>
#include <kernel/kernel.h>

#include <kernel/bus/bus.h>


#ifdef BOARD_RETROBOARD_REV2

// Caching

#define CACHE_SIZE   32

uint8_t cache[CACHE_SIZE];

uint8_t dirty_bits[CACHE_SIZE];

uint32_t cache_begin = 0;
uint32_t cache_end   = 0;


// NOTE: Address and data buses are multiplexed requiring external logic
#define _BUS_LOWER_PORT_A__     // Define port A as the lower address bus
#define _BUS_MIDDLE_PORT_C__    // Define Port C as the middle address bus
#define _BUS_UPPER_PORT_D__     // Define port D as the upper address bus
#define _BUS_CONTROL_PORT_B__   // Define port B as the control bus

// Control signals
// NOTE: Signals are specific to external logic requirements
#define _CONTROL_DATA_DIRECTION__     0x00 // Data direction signal
#define _CONTROL_BUS_ENABLE__         0x01 // Bus enable
#define _CONTROL_CLOCK__              0x02 // Peripheral device clock
#define _CONTROL_ADDRESS_LATCH__      0x03 // Address latch enable
#define _CONTROL_READ__               0x04 // Read signal
#define _CONTROL_WRITE__              0x05 // Write signal
#define _CONTROL_UNUSED_A__           0x06 // Unused
#define _CONTROL_UNUSED_B__           0x07 // Unused

// Control bytes
#define _CONTROL_READ_CYCLE__       0b00100100
#define _CONTROL_WRITE_CYCLE__      0b00010101
#define _CONTROL_READ_LATCH__       0b00110100
#define _CONTROL_WRITE_LATCH__      0b00110101
#define _CONTROL_OPEN_LATCH__       0b00111111
#define _CONTROL_CLOSED_LATCH__     0b00111101



// Lower address bus
#ifdef _BUS_LOWER_PORT_A__
  #define _BUS_LOWER_DIR__   DDRA
  #define _BUS_LOWER_OUT__   PORTA
  #define _BUS_LOWER_IN__    PINA
#endif
#ifdef _BUS_LOWER_PORT_B__
  #define _BUS_LOWER_DIR__   DDRB
  #define _BUS_LOWER_OUT__   PORTB
  #define _BUS_LOWER_IN__    PINB
#endif
#ifdef _BUS_LOWER_PORT_C__
  #define _BUS_LOWER_DIR__   DDRC
  #define _BUS_LOWER_OUT__   PORTC
  #define _BUS_LOWER_IN__    PINC
#endif
#ifdef _BUS_LOWER_PORT_D__
  #define _BUS_LOWER_DIR__   DDRD
  #define _BUS_LOWER_OUT__   PORTD
  #define _BUS_LOWER_IN__    PIND
#endif

// Middle address bus
#ifdef _BUS_MIDDLE_PORT_A__
  #define _BUS_MIDDLE_DIR__  DDRA
  #define _BUS_MIDDLE_OUT__  PORTA
  #define _BUS_MIDDLE_IN__   PINA
#endif
#ifdef _BUS_MIDDLE_PORT_B__
  #define _BUS_MIDDLE_DIR__  DDRB
  #define _BUS_MIDDLE_OUT__  PORTB
  #define _BUS_MIDDLE_IN__   PINB
#endif
#ifdef _BUS_MIDDLE_PORT_C__
  #define _BUS_MIDDLE_DIR__  DDRC
  #define _BUS_MIDDLE_OUT__  PORTC
  #define _BUS_MIDDLE_IN__   PINC
#endif
#ifdef _BUS_MIDDLE_PORT_D__
  #define _BUS_MIDDLE_DIR__  DDRD
  #define _BUS_MIDDLE_OUT__  PORTD
  #define _BUS_MIDDLE_IN__   PIND
#endif

// Upper address bus
#ifdef _BUS_UPPER_PORT_A__
  #define _BUS_UPPER_DIR__   DDRA
  #define _BUS_UPPER_OUT__   PORTA
  #define _BUS_UPPER_IN__    PINA
#endif
#ifdef _BUS_UPPER_PORT_B__
  #define _BUS_UPPER_DIR__   DDRB
  #define _BUS_UPPER_OUT__   PORTB
  #define _BUS_UPPER_IN__    PINB
#endif
#ifdef _BUS_UPPER_PORT_C__
  #define _BUS_UPPER_DIR__   DDRC
  #define _BUS_UPPER_OUT__   PORTC
  #define _BUS_UPPER_IN__    PINC
#endif
#ifdef _BUS_UPPER_PORT_D__
  #define _BUS_UPPER_DIR__   DDRD
  #define _BUS_UPPER_OUT__   PORTD
  #define _BUS_UPPER_IN__    PIND
#endif

// Control bus
#ifdef _BUS_CONTROL_PORT_A__
  #define _CONTROL_DIR__     DDRA
  #define _CONTROL_OUT__     PORTA
  #define _CONTROL_IN__      PINA
#endif
#ifdef _BUS_CONTROL_PORT_B__
  #define _CONTROL_DIR__     DDRB
  #define _CONTROL_OUT__     PORTB
  #define _CONTROL_IN__      PINB
#endif
#ifdef _BUS_CONTROL_PORT_C__
  #define _CONTROL_DIR__     DDRC
  #define _CONTROL_OUT__     PORTC
  #define _CONTROL_IN__      PINC
#endif
#ifdef _BUS_CONTROL_PORT_D__
  #define _CONTROL_DIR__     DDRD
  #define _CONTROL_OUT__     PORTD
  #define _CONTROL_IN__      PIND
#endif

void bus_initiate(void) {
    // Set internal pull-up resistors
    _BUS_LOWER_DIR__ = 0x00;
	_BUS_LOWER_OUT__ = 0xff;
	_BUS_LOWER_DIR__ = 0xff;
	bus_control_zero();
    bus_address_zero();
    return;
}

void bus_control_zero(void) {
    _CONTROL_DIR__=0xff;
	_CONTROL_OUT__=0xff;
	return;
}

void bus_address_zero(void) {
	_BUS_LOWER_DIR__=0xff;
	_BUS_LOWER_OUT__=0xff;
	
	_BUS_MIDDLE_DIR__=0xff;
	_BUS_MIDDLE_OUT__=0xff;
	
	_BUS_UPPER_DIR__=0x0f;
	_BUS_UPPER_OUT__=0xff;
	return;
}

void bus_raw_read_memory(struct Bus* bus, uint32_t address, uint8_t* buffer) {
	
	// Address the device
	_BUS_UPPER_OUT__  = (address >> 16) & 0xff;
	_BUS_MIDDLE_OUT__ = (address >> 8) & 0xff;
	_BUS_LOWER_OUT__  = address & 0xff;
	
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
	
	// Read the data byte
	*buffer = _BUS_LOWER_IN__;
	
	// End cycle (reset the select logic)
	_CONTROL_OUT__ = _CONTROL_OPEN_LATCH__;
	_BUS_UPPER_OUT__  = 0x00;
	_CONTROL_OUT__ = _CONTROL_CLOSED_LATCH__;
	_BUS_LOWER_DIR__ = 0xff;
	
	return;
}


void bus_raw_write_memory(struct Bus* bus, uint32_t address, uint8_t byte) {
	
	// Address the device
	_BUS_UPPER_OUT__  = (address >> 16) & 0xff;
	_BUS_MIDDLE_OUT__ = (address >> 8) & 0xff;
	_BUS_LOWER_OUT__  = address & 0xff;
	
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
	
	return;
}

void bus_read_byte(struct Bus* bus, uint32_t address, uint8_t* buffer) {
    bus_raw_read_memory(bus, address, buffer);
	return;
}

void bus_write_byte(struct Bus* bus, uint32_t address, uint8_t byte) {
    bus_raw_write_memory(bus, address, byte);
    return;
}


void bus_write_byte_eeprom(struct Bus* bus, uint32_t address, uint8_t byte) {
    bus_raw_write_memory(bus, address, byte);
    _delay_ms(10);
	return;
}


// Cache

void bus_flush_cache(struct Bus* bus, uint32_t address) {
    // Write back only modified cache bytes
    for (uint32_t i = 0; i < CACHE_SIZE; i++) {
        if (dirty_bits[i]) {  // Only write if modified
            bus_raw_write_memory(bus, cache_begin + i, cache[i]);
            dirty_bits[i] = 0; // Clear dirty bit after writing
        }
    }
    
    // Fetch new data
    for (uint32_t i = 0; i < CACHE_SIZE; i++) {
        bus_raw_read_memory(bus, address + i, &cache[i]);
        dirty_bits[i] = 0; // Reset dirty state for new data
    }
    
    cache_begin = address;
    cache_end = address + CACHE_SIZE;
    return;
}

void bus_read_memory(struct Bus* bus, uint32_t address, uint8_t* buffer) {
	if (address >= cache_begin && address < cache_end) {
        *buffer = cache[address - cache_begin];
        return;
    }
    
    bus_flush_cache(bus, address);
    *buffer = cache[0];
    return;
}


void bus_write_memory(struct Bus* bus, uint32_t address, uint8_t byte) {
	if (address >= cache_begin && address < cache_end) {
        uint32_t offset = address - cache_begin;
        cache[offset] = byte;
        dirty_bits[offset] = 1;  // Mark as modified
        return;
    }
    
    bus_flush_cache(bus, address);
    cache[0] = byte;
    dirty_bits[0] = 1;
    
    return;
}



#endif

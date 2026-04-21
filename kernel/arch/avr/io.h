#ifndef AVR_IO_INTERFACE_H
#define AVR_IO_INTERFACE_H

#include <avr/io.h>
#include <stdint.h>

#include <kernel/bus/bus.h>
#include <kernel/delay.h>

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


static inline void mmio_control_zero(void) {
    _CONTROL_DIR__=0xff;
	_CONTROL_OUT__=0xff;
}

static inline void mmio_address_zero(void) {
	_BUS_LOWER_DIR__=0xff;
	_BUS_LOWER_OUT__=0xff;
	
	_BUS_MIDDLE_DIR__=0xff;
	_BUS_MIDDLE_OUT__=0xff;
	
	_BUS_UPPER_DIR__=0x0f;
	_BUS_UPPER_OUT__=0xff;
}

static inline void mmio_initiate(void) {
    _BUS_LOWER_DIR__ = 0x00;
	_BUS_LOWER_OUT__ = 0xff; // Set internal pull-up resistors
	_BUS_LOWER_DIR__ = 0xff;
	mmio_control_zero();
    mmio_address_zero();
}

static inline void mmio_readb(struct Bus* bus, uint32_t address, uint8_t* byte) {
    _BUS_UPPER_OUT__  = (address >> 16) & 0xff;
	_BUS_MIDDLE_OUT__ = (address >> 8) & 0xff;
	_BUS_LOWER_OUT__  =  address & 0xff;
	
    _CONTROL_OUT__ = _CONTROL_READ_LATCH__;
	_BUS_LOWER_DIR__ = 0x00;
	_CONTROL_OUT__ = _CONTROL_READ_CYCLE__;
	
	uint16_t wait = bus->read_waitstate;
    while (wait--) __asm__ volatile("nop");
	
	*byte = _BUS_LOWER_IN__;
	
	_CONTROL_OUT__ = _CONTROL_OPEN_LATCH__;
	_BUS_UPPER_OUT__  = 0x00;
	_BUS_LOWER_DIR__ = 0xff;
}

static inline void mmio_writeb(struct Bus* bus, uint32_t address, uint8_t* byte) {
    _BUS_UPPER_OUT__  = (address >> 16) & 0xff;
	_BUS_MIDDLE_OUT__ = (address >> 8) & 0xff;
	_BUS_LOWER_OUT__  =  address & 0xff;
	
	_CONTROL_OUT__ = _CONTROL_WRITE_LATCH__;
	_BUS_LOWER_OUT__ = *byte;
	_CONTROL_OUT__ = _CONTROL_WRITE_CYCLE__;
	
	uint16_t wait = bus->write_waitstate;
    while (wait--) __asm__ volatile("nop");
	
	_CONTROL_OUT__ = _CONTROL_OPEN_LATCH__;
	_BUS_UPPER_OUT__  = 0x00;
	
	__asm__ volatile("nop");
	__asm__ volatile("nop");
}

static inline void mmio_writeb_eeprom(struct Bus* bus, uint32_t address, uint8_t* byte) {
    mmio_writeb(bus, address, byte);
    _delay_ms(5);
}

static inline void mmio_read_block(struct Bus* bus, uint32_t address, uint8_t* byte, uint32_t size) {
    for (uint32_t i=0; i < size; i++) {
        uint32_t block_address = address + i;
        _BUS_UPPER_OUT__  = (block_address >> 16) & 0xff;
        _BUS_MIDDLE_OUT__ = (block_address >> 8) & 0xff;
        _BUS_LOWER_OUT__  =  block_address & 0xff;
        
        _CONTROL_OUT__ = _CONTROL_READ_LATCH__;
        _BUS_LOWER_DIR__ = 0x00;
        _CONTROL_OUT__ = _CONTROL_READ_CYCLE__;
        
        __asm__ volatile("nop");
        __asm__ volatile("nop");
        
        byte[i] = _BUS_LOWER_IN__;
        
        _CONTROL_OUT__ = _CONTROL_OPEN_LATCH__;
        _BUS_LOWER_DIR__ = 0xff;
    }
	
	_CONTROL_OUT__ = _CONTROL_OPEN_LATCH__;
    _BUS_LOWER_DIR__ = 0xff;
}

#endif

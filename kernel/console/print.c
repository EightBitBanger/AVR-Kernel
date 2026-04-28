#include <kernel/boot/avr/interrupt.h>

#include <kernel/kernel.h>
#include <kernel/bus/bus.h>
#include <kernel/arch/avr/io.h>

#include <kernel/console/virtual_key.h>
#include <kernel/console/display.h>
#include <kernel/console/print.h>

#include <kernel/string.h>
#include <ctype.h>
#include <string.h>

extern uint8_t cursor_position;
extern uint8_t cursor_line;

void print(const char* str) {
    display_busy_wait();
    
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    while (*str) {
        uint8_t ch = (uint8_t)*str++;
        
        // New line
        if (ch == '\n') {
            cursor_position = 0;
            
            if (cursor_line < (display_get_height() - 1)) {
                cursor_line++;
            } else {
                uint8_t one = 1;
                mmio_writeb(&bus, display_get_device_address() + 0x00005, &one);
                display_busy_wait();
            }
            
            continue;
        } else {
            
            if (display_cursor_get_position() >= display_get_width() - 1) {
                
                display_cursor_set_position(cursor_position);
                display_cursor_set_line(cursor_line);
                
                display_putc(ch);
                
                cursor_position = 0;
                
                if (display_cursor_get_line() < (display_get_height() - 1)) {
                    cursor_line++;
                } else {
                    // New line / shift up
                    uint8_t one = 1;
                    mmio_writeb(&bus, display_get_device_address() + 0x00005, &one);
                    display_busy_wait();
                }
                
                continue;
            }
        }
        
        display_cursor_set_position(cursor_position);
        display_cursor_set_line(cursor_line);
        display_putc(ch);
        
        cursor_position++;
    }
    
    display_cursor_set_position(cursor_position);
    display_cursor_set_line(cursor_line);
}

void print_int(int32_t value) {
    char number_string[12];
    utos(value, number_string);
    
    print(number_string);
}

void print_hex(uint8_t value) {
    char buffer[3];
    for (int i = 0; i < 2; i++) {
        // Extract high nibble first, then low nibble
        uint8_t nibble = (value >> (4 - (i * 4))) & 0x0F;
        if (nibble < 10) {
            buffer[i] = '0' + nibble;
        } else {
            buffer[i] = 'A' + (nibble - 10);
        }
    }
    buffer[2] = '\0';
    print(buffer);
}

void print_hex16(uint16_t value) {
    for (int i = 1; i >= 0; i--) {
        uint8_t byte = (uint8_t)(value >> (i * 8));
        print_hex(byte);
    }
}

void print_hex32(uint32_t value) {
    for (int i = 3; i >= 0; i--) {
        uint8_t byte = (uint8_t)(value >> (i * 8));
        print_hex(byte);
    }
}

void print_bits(uint8_t value) {
    char buffer[9]; 
    for (int i = 0; i < 8; i++) {
        buffer[i] = ((value >> (7 - i)) & 1) ? '1' : '0';
    }
    buffer[8] = '\0';
    print(buffer);
}

void print_bits16(uint16_t value) {
    for (int i = 1; i >= 0; i--) {
        uint8_t byte = (uint8_t)(value >> (i * 8));
        print_bits(byte);
    }
}

void print_bits32(uint32_t value) {
    for (int i = 3; i >= 0; i--) {
        uint8_t byte = (uint8_t)(value >> (i * 8));
        print_bits(byte);
    }
}

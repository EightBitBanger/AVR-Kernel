#include <kernel/boot/avr/interrupt.h>

#include <kernel/kernel.h>
#include <kernel/bus/bus.h>
#include <kernel/arch/avr/io.h>

#include <kernel/console/virtual_key.h>
#include <kernel/console/keyboard.h>
#include <kernel/console/console.h>
#include <kernel/console/print.h>

#include <ctype.h>

#include <stdbool.h>
#include <stdint.h>

struct Bus display_bus;

const char* display_device_name = "display";

uint32_t display_address;

uint8_t cursor_position;
uint8_t cursor_line;

uint8_t display_width;
uint8_t display_height;

void display_init(void) {
    display_bus.write_waitstate = 20;
    display_bus.read_waitstate  = 20;
    
    display_width  = 21;
    display_height = 8;
    
    display_address = device_get_hardware_address(display_device_name);
    
    if (display_address == 0) 
        return;
    
    
    
    
    
    /*
    char buffer[17];
    device_get_hardware_data(display_address, buffer, 16);
    
    buffer[16] = '\0';
    
    print(buffer);
    */
    
    
    
    //while(1);
}

uint32_t display_get_device_address(void) {
    return display_address;
}

void display_busy_wait(void) {
    if (display_address == 0) 
        return;
    uint8_t checkByte = 0;
    mmio_readb(&display_bus, display_address, &checkByte);
    while (checkByte != 0x10) {
        mmio_readb(&display_bus, display_address, &checkByte);
    }
}

uint16_t display_get_width(void) {
    return display_width;
}

uint16_t display_get_height(void) {
    return display_height;
}


void display_putc(const char ch) {
    display_busy_wait();
    mmio_writeb(&display_bus, display_address + 0x00010, (uint8_t*)&ch);
}

void display_newline(void) {
    uint8_t one = 1;
    mmio_writeb(&display_bus, display_address + 0x00005, &one);
    display_busy_wait();
}

void display_cursor_set_position(uint16_t position) {
    display_busy_wait();
    if (position >= display_get_width()) position = display_width - 1;
    cursor_position = position;
    mmio_writeb(&display_bus, display_address + 0x00002, &cursor_position);
}

void display_cursor_set_line(uint16_t line) {
    display_busy_wait();
    if (line >= display_get_height()) line = display_height - 1;
    cursor_line = line;
    mmio_writeb(&display_bus, display_address + 0x00001, &cursor_line);
}

uint16_t display_cursor_get_position(void) {
    return cursor_position;
}

uint16_t display_cursor_get_line(void) {
    return cursor_line;
}

void display_clear(void) {
    display_busy_wait();
    display_cursor_set_position(0);
    display_cursor_set_line(0);
    
    for (uint8_t h=0; h < display_height; h++) 
        for (uint8_t w=0; w < display_width; w++) 
            print(" ");
    display_cursor_set_position(0);
    display_cursor_set_line(0);
    return;
}

void console_set_blink_rate(uint8_t rate) {
    display_busy_wait();
    mmio_writeb(&display_bus, display_address + 0x00003, &rate);
}

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

uint8_t cursor_position;
uint8_t cursor_line;

uint8_t display_width;
uint8_t display_height;

#include <kernel/arch/x86/io.h>

#include <kernel/console/print.h>
#include <kernel/console/display.h>

#define CRTC_INDEX_PORT 0x3D4
#define CRTC_DATA_PORT  0x3D5

volatile uint16_t* volatile display_address = (volatile uint16_t*)0xB8000;



#define VGA_COLOR_WHITE_ON_BLACK 0x0F

static inline void x86_sync_hardware_cursor(void) {
    // Calculate the flat 1D index: (Row * Width) + Column
    uint16_t flat_position = (cursor_line * display_width) + cursor_position;
    
    // Send the high byte to Register 0x0E
    outb(CRTC_INDEX_PORT, 0x0E);
    outb(CRTC_DATA_PORT,  (uint8_t)((flat_position >> 8) & 0xFF));
    
    // Send the low byte to Register 0x0F
    outb(CRTC_INDEX_PORT, 0x0F);
    outb(CRTC_DATA_PORT,  (uint8_t)(flat_position & 0xFF));
}



void display_init(void) {
    display_width  = 80;
    display_height = 25;
    
    cursor_line = 0;
    cursor_position = 0;
}

uint32_t display_get_device_address(void) {
    return *display_address;
}

void display_busy_wait(void) {
}

uint16_t display_get_width(void) {
    return display_width;
}

uint16_t display_get_height(void) {
    return display_height;
}


void display_putc(const char ch) {
    int index = (cursor_line * display_get_width()) + cursor_position;
    display_address[index] = (uint16_t)ch | ((uint16_t)VGA_COLOR_WHITE_ON_BLACK << 8);
}

void display_newline(void) {
    for (int y = 1; y < display_get_height(); y++) {
        for (int x = 0; x < display_get_width(); x++) {
            display_address[(y - 1) * display_get_width() + x] = display_address[y * display_get_width() + x];
        }
    }
    
    for (int x = 0; x < display_get_width(); x++) 
        display_address[(display_get_height() - 1) * display_get_width() + x] = (uint16_t)' ' | ((uint16_t)VGA_COLOR_WHITE_ON_BLACK << 8);
}

void display_cursor_set_position(uint16_t position) {
    if (position >= display_width) position = display_width - 1;
    cursor_position = (uint8_t)position;
    x86_sync_hardware_cursor();
}

void display_cursor_set_line(uint16_t line) {
    if (line >= display_height) line = display_height - 1; 
    cursor_line = (uint8_t)line;
    x86_sync_hardware_cursor();
}

uint16_t display_cursor_get_position(void) {
    return cursor_position;
}

uint16_t display_cursor_get_line(void) {
    return cursor_line;
}

void display_clear(void) {
    for (int i = 0; i < display_width * display_height; i++) 
        display_address[i] = (uint16_t)' ' | (uint16_t)(15 | (0 << 4)) << 8;
}

void console_set_blink_rate(uint8_t rate) {
    outb(CRTC_INDEX_PORT, 0x0A);
    uint8_t reg_val = inb(CRTC_DATA_PORT);
    
    if (rate == 0) {
        reg_val |= 0x20; // Disable cursor
    } else {
        reg_val &= ~0x20; // Enable cursor
    }
    
    outb(CRTC_INDEX_PORT, 0x0A);
    outb(CRTC_DATA_PORT, reg_val);
}

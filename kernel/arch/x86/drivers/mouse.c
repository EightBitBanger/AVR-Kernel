#include <ctype.h>
#include <stdint.h>
#include <kernel/arch/x86/io.h>
#include <kernel/boot/x86/interrupt.h>
#include <kernel/console/display.h>
#include <kernel/console/mouse.h>

static int8_t mouse_packet[3]; 

bool mouse_left_clicked   = false;
bool mouse_right_clicked  = false;
bool mouse_middle_clicked = false;

int32_t fixed_mouse_x = 0;
int32_t fixed_mouse_y = 0;

int32_t horizontal_precision = 32;
int32_t vertical_precision   = 32;

int32_t horizontal_mul = 3;
int32_t vertical_mul   = 3;

int32_t acceleration_max = 2;

uint8_t mouse_cycle = 0;

int32_t mouse_x = 0;
int32_t mouse_y = 0;

Point mouse_get_position(void) {
    return (Point){mouse_x, mouse_y};
}

bool mouse_get_button(uint8_t button) {
    switch(button) {
        case MOUSE_BUTTON_LEFT:   return mouse_left_clicked;
        case MOUSE_BUTTON_MIDDLE: return mouse_middle_clicked;
        case MOUSE_BUTTON_RIGHT:  return mouse_right_clicked;
    }
    return false;
}

void mouse_set_position(uint32_t x, uint32_t y) {
    mouse_x = x;
    mouse_y = y;
    fixed_mouse_x = (int32_t)x << 8;
    fixed_mouse_y = (int32_t)y << 8;
}

void mouse_set_cursor_speed(int32_t horz, int32_t vert) {
    horizontal_mul = horz;
    vertical_mul   = vert;
}

void mouse_set_cursor_acceleration(int32_t acceleration) {
    acceleration_max = acceleration;
}

void mouse_wait(uint8_t type) {
    if (type == 0) {
        while ((inb(0x64) & 1) == 0); // Wait for data to be readable
    } else {
        while ((inb(0x64) & 2) != 0); // Wait for buffer to clear for writing
    }
}

void mouse_write(uint8_t write) {
    mouse_wait(1);
    outb(0x64, 0xD4); // Signal: Sending command directly to mouse
    mouse_wait(1);
    outb(0x60, write);
}

uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_initiate(void) {
    uint8_t status;
    
    mouse_wait(1);
    outb(0x64, 0xA8); 
    
    mouse_wait(1);
    outb(0x64, 0x20); // Command: Read Command Byte
    mouse_wait(0);
    status = inb(0x60);
    
    status &= ~(1 << 0); // Clear Bit 0 (Disable Keyboard Interrupt / IRQ 1)
    status &= ~(1 << 1); // Clear Bit 1 (Disable Mouse Interrupt / IRQ 12)
    status |=  (1 << 5); // Ensure bit 5 is set
    
    // Write the modified Configuration Byte back
    mouse_wait(1);
    outb(0x64, 0x60); // Command: Write Command Byte
    mouse_wait(1);
    outb(0x60, status);
    
    // Tell the mouse hardware to use default configuration
    mouse_write(0xF6);
    mouse_read(); // Clear the Acknowledge byte (0xFA)
    
    // Tell the mouse hardware to start streaming data packets
    mouse_write(0xF4);
    mouse_read(); // Clear the Acknowledge byte (0xFA)
}

void mouse_event_handler(void) {
    uint8_t input_byte = inb(0x60);
    
    mouse_packet[mouse_cycle] = (int8_t)input_byte;
    mouse_cycle++;
    
    if (mouse_cycle == 3) {
        mouse_cycle = 0;
        
        if ((mouse_packet[0] & 0x08) == 0) 
            return; 
        
        mouse_left_clicked   = (mouse_packet[0] & 0x01) ? true : false;
        mouse_right_clicked  = (mouse_packet[0] & 0x02) ? true : false;
        mouse_middle_clicked = (mouse_packet[0] & 0x04) ? true : false;
        
        int16_t rel_x = (int16_t)mouse_packet[1];
        int16_t rel_y = (int16_t)mouse_packet[2];
        
        if (mouse_packet[0] & 0x10) rel_x |= 0xFF00;
        if (mouse_packet[0] & 0x20) rel_y |= 0xFF00;
        
        // Calculate dynamic acceleration
        
        int16_t abs_x = rel_x < 0 ? -rel_x : rel_x;
        int16_t abs_y = rel_y < 0 ? -rel_y : rel_y;
        
        int32_t current_mul_x = horizontal_precision;
        int32_t current_mul_y = vertical_precision;
        
        int32_t horz_precision = horizontal_precision;
        int32_t vert_precision = vertical_precision;
        
        int32_t horz_mul = horizontal_mul;
        int32_t vert_mul = vertical_mul;
        
        if (abs_x > acceleration_max) abs_x = acceleration_max;
        if (abs_y > acceleration_max) abs_y = acceleration_max;
        
        current_mul_x = ((horz_precision * horz_mul) * abs_x) >> 1;
        current_mul_y = ((vert_precision * vert_mul) * abs_y) >> 1;
        
        fixed_mouse_x += (int32_t)rel_x * current_mul_x;
        fixed_mouse_y -= (int32_t)rel_y * current_mul_y;
        
        // Use -1 to prevent overflow tracking past screen boundaries
        int32_t max_x = ((int32_t)display_get_width() << 8) - 1;
        int32_t max_y = ((int32_t)display_get_height() << 8) - 1;
        
        if (fixed_mouse_x < 0) fixed_mouse_x = 0;
        if (fixed_mouse_y < 0) fixed_mouse_y = 0;
        if (fixed_mouse_x > max_x) fixed_mouse_x = max_x;
        if (fixed_mouse_y > max_y) fixed_mouse_y = max_y;
        
        mouse_x = fixed_mouse_x >> 8;
        mouse_y = fixed_mouse_y >> 8;
    }
}

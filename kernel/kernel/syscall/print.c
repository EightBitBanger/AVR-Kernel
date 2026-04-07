#include <kernel/boot/avr/interrupt.h>

#include <kernel/kernel.h>
#include <kernel/bus/bus.h>
#include <kernel/arch/avr/io.h>

#include <kernel/device/device.h>

#include <kernel/syscall/console.h>
#include <kernel/syscall/print.h>

#include <string.h>

uint32_t display_address;

static uint8_t display_width  = 21;
static uint8_t display_height = 8;

static uint8_t console_position = 0;
static uint8_t console_line     = 0;

static volatile char    current_character = 0x00;
static volatile char    last_character    = 0x00;
static volatile uint8_t key_ready         = 0;

static char    keyboard_string[16];
static uint8_t keyboard_length = 0;

static char* prompt_string = "/>";
static uint8_t prompt_length = sizeof(prompt_string);

char kb_getc(void);

void kb_isr_callback(void) {
    current_character = kb_getc();
    
    if (last_character == current_character)
        return;
    
    last_character = current_character;
    key_ready      = 1;
}


void kb_handler(void) {
    char ch;
    
    if (key_ready == 0)
        return;
    
    interrupt_disable();
    
    ch = current_character;
    key_ready = 0;
    
    interrupt_enable();
    
    if (ch == 0x00)
        return;
    
    // Backspace
    if (ch == 0x01) {
        if (keyboard_length > 0) {
            keyboard_length--;
            keyboard_string[keyboard_length] = '\0';
            
            print("\x01");
        }
        
        return;
    }
    
    // Enter
    if (ch == 0x02) {
        keyboard_string[keyboard_length] = '\0';
        print("\n");
        
        if (keyboard_length > 0) {
            process_command((char*)keyboard_string);
            
            if (console_position != 0) 
                print("\n");
        }
        
        keyboard_length = 0;
        keyboard_string[0] = '\0';
        
        print(prompt_string);
        return;
    }
    
    if (ch < 0x20 || ch == 0x7f)
        return;
    
    if (keyboard_length < (sizeof(keyboard_string) - 1)) {
        keyboard_string[keyboard_length] = ch;
        keyboard_length++;
        keyboard_string[keyboard_length] = '\0';
        
        char input[2];
        input[0] = ch;
        input[1] = '\0';
        
        print(input);
    }
}

void console_init(void) {
    current_character = kb_getc();
    last_character = current_character;
    display_address = device_get_hardware_address("display");
    
    //if (display_address == 0) 
    //    return;
    
    keyboard_length = 0;
    memset(keyboard_string, 0x00, sizeof(keyboard_string));
}

void console_busy_wait(uint32_t address) {
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    uint8_t checkByte = 0;
    mmio_readb(&bus, address, &checkByte);
    while (checkByte != 0x10) {
        mmio_readb(&bus, address, &checkByte);
    }
}

void console_set_cursor(uint8_t x, uint8_t y) {
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    // Boundary checks to prevent memory corruption/glitches
    if (x >= display_width) x = display_width - 1;
    if (y >= display_height) y = display_height - 1;
    
    console_position = x;
    console_line = y;
    
    // Update the hardware MMIO registers immediately
    mmio_writeb(&bus, display_address + 0x00002, &console_position);
    mmio_writeb(&bus, display_address + 0x00001, &console_line);
}

void print_prompt(void) {
    print(prompt_string);
}

void console_cursor_blink_rate(uint8_t rate) {
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    mmio_writeb(&bus, display_address + 0x00003, &rate);
}

void console_clear(void) {
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    mmio_writeb(&bus, display_address + 0x00009, (uint8_t*)1);
}

uint16_t display_get_width(void) {
    return display_width;
}

uint16_t display_get_height(void) {
    return display_width;
}

void print(const char* str) {
    console_busy_wait(display_address);
    
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    while (*str) {
        uint8_t ch = (uint8_t)*str++;
        
        // Backspace
        if (ch == 0x01) {
            if (console_position > 0) {
                console_position--;
                mmio_writeb(&bus, display_address + 0x00002, &console_position);
                
                uint8_t* sp = " ";
                mmio_writeb(&bus, display_address + 0x00010, sp);
            }
            continue;
        }
        
        // New line
        if (ch == '\n') {
            console_position = 0;
            
            if (console_line < (display_height - 1)) {
                console_line++;
            } else {
                uint8_t one = 1;
                mmio_writeb(&bus, display_address + 0x00005, &one);
                console_busy_wait(display_address);
            }
            
            continue;
        } else {
            
            if (console_position >= display_width) {
                console_position = 0;
                
                if (console_line < (display_height - 1)) {
                    console_line++;
                } else {
                    uint8_t one = 1;
                    mmio_writeb(&bus, display_address + 0x00005, &one);
                    console_busy_wait(display_address);
                }
            }
        }
        
        mmio_writeb(&bus, display_address + 0x00002, &console_position);
        mmio_writeb(&bus, display_address + 0x00001, &console_line);
        mmio_writeb(&bus, display_address + 0x00010, &ch);
        
        console_position++;
    }
    
    mmio_writeb(&bus, display_address + 0x00002, &console_position);
    mmio_writeb(&bus, display_address + 0x00001, &console_line);
}

void print_int(int32_t value) {
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    char buffer[12]; // Enough for -2147483648 and null terminator
    int8_t i = 0;
    uint32_t n;
    
    // Handle 0 explicitly
    if (value == 0) {
        print("0");
        return;
    }
    
    // Handle negative numbers
    if (value < 0) {
        print("-");
        // Use unsigned to avoid overflow issues with INT_MIN
        n = (uint32_t)-value;
    } else {
        n = (uint32_t)value;
    }
    
    // Extract digits in reverse order
    while (n > 0) {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }
    
    // Reverse the digits in place to print correctly
    char reversed[12];
    int8_t j = 0;
    while (i > 0) {
        reversed[j++] = buffer[--i];
    }
    reversed[j] = '\0';
    
    print(reversed);
}


char kb_getc(void) {
    struct Bus bus;
    bus.write_waitstate = 1;
    bus.read_waitstate  = 2;
    
    uint8_t scancode_low;
    uint8_t scancode_high;
    mmio_readb(&bus, 0x90000, &scancode_low);
    mmio_readb(&bus, 0x90001, &scancode_high);
    
    if ( (scancode_low == 0x98)|(scancode_low == 0x99)|(scancode_low == 0x92)|(scancode_low == 0x91)|(scancode_low == 0x90)|(scancode_low == 0x9a)|(scancode_low == 0x9b) ) {
		if (scancode_high == 0xc4) {return 0x11;} // Left shift pressed
	}
	
	if ( (scancode_low == 0x58)|(scancode_low == 0x59)|(scancode_low == 0x52)|(scancode_low == 0x51)|(scancode_low == 0x50)|(scancode_low == 0x5a)|(scancode_low == 0x5b) ) {
		if (scancode_high == 0xd6) {return 0x11;} // Right shift pressed
	}
	
	if (scancode_low == 0xdf) {
		if (scancode_high == 0x9a) {return 0x05;} // Left arrow
		if (scancode_high == 0x96) {return ']';}  // Right square bracket
		if (scancode_high == 0x90) {return 'i';}
		if (scancode_high == 0xc6) {return 's';}
		if (scancode_high == 0x88) {return 'd';}
		if (scancode_high == 0xca) {return 'f';}
		if (scancode_high == 0xcc) {return 'h';}
		if (scancode_high == 0x8e) {return 'j';}
		if (scancode_high == 0xd2) {return 'l';}
		return  0x00;
	}
	if (scancode_low == 0x9f) {
		if (scancode_high == 0xc4) {return 0x12;} // Shift left released
		if (scancode_high == 0xd9) {return 0x01;} // Backspace
		if (scancode_high == 0xd6) {return 0x02;} // Enter
		if (scancode_high == 0xdc) {return 0x04;} // Down arrow
		if (scancode_high == 0xd3) {return '-';}  // Dash
		if (scancode_high == 0x83) {return '`';}  // Apostrophe
		if (scancode_high == 0x9d) {return 0x07;} // Escape
		if (scancode_high == 0x92) {return '/';}  // Forward slash
		if (scancode_high == 0x94) {return 0x27;} // '
		if (scancode_high == 0x91) {return '9';}
		if (scancode_high == 0x8f) {return '8';}
		if (scancode_high == 0xcd) {return '6';}
		if (scancode_high == 0xcb) {return '5';}
		if (scancode_high == 0x89) {return '3';}
		if (scancode_high == 0xc7) {return '2';}
		if (scancode_high == 0x85) {return '1';}
		if (scancode_high == 0x86) {return 'z';}
		if (scancode_high == 0xc8) {return 'x';}
		if (scancode_high == 0x8a) {return 'v';}
		if (scancode_high == 0x8c) {return 'b';}
		if (scancode_high == 0xce) {return 'm';}
		if (scancode_high == 0xd0) {return 'k';}
		return  0x00;
	}
	if (scancode_low == 0x5f) {
		if (scancode_high == 0xd6) {return 0x12;} // Right shift released
		if (scancode_high == 0x9d) {return 0x03;} // Up arrow
		if (scancode_high == 0x8a) {return 0x20;} // Space
		if (scancode_high == 0xc4) {return 0x09;} // Alt
		if (scancode_high == 0xd5) {return '=';}  // Equals
		if (scancode_high == 0xdc) {return 0x10;} // Delete
		if (scancode_high == 0xd0) {return ',';}  // Comma
		if (scancode_high == 0x92) {return '.';}  // Period
		if (scancode_high == 0x97) {return 0x5c;} // Backslash
		if (scancode_high == 0x91) {return '0';}
		if (scancode_high == 0x8f) {return '7';}
		if (scancode_high == 0x89) {return '4';}
		if (scancode_high == 0xc8) {return 'c';}
		if (scancode_high == 0x8c) {return 'n';}
		if (scancode_high == 0x85) {return 'q';}
		if (scancode_high == 0xc7) {return 'w';}
		if (scancode_high == 0xcb) {return 'r';}
		if (scancode_high == 0xcd) {return 'y';}
		if (scancode_high == 0xd3) {return 'p';}
		return  0x00;
	}
	if (scancode_low == 0x1f) {
		if (scancode_high == 0xdd) {return 0x06;} // Right arrow
		if (scancode_high == 0xc5) {return 0x08;} // Control
		if (scancode_high == 0x93) {return ';';}  // Semi-colon
		if (scancode_high == 0x95) {return '[';}  // Left square bracket
		if (scancode_high == 0xc9) {return 'e';}
		if (scancode_high == 0x8b) {return 't';}
		if (scancode_high == 0xcf) {return 'u';}
		if (scancode_high == 0xd1) {return 'o';}
		if (scancode_high == 0x87) {return 'a';}
		if (scancode_high == 0x8d) {return 'g';}
		return  0x00;
	}
	return 0x00;
}

#include <kernel/boot/avr/interrupt.h>

#include <kernel/kernel.h>
#include <kernel/mutex.h>
#include <kernel/bus/bus.h>
#include <kernel/arch/avr/io.h>

#include <kernel/device/device.h>

#include <kernel/console/console.h>
#include <kernel/console/print.h>

#include <kernel/string.h>

extern uint32_t display_address;

extern uint8_t console_position;
extern uint8_t console_line;

static volatile char    current_character = 0x00;
static volatile char    last_character    = 0x00;
static volatile uint8_t key_ready         = 0;

extern char* keyboard_string;
extern uint8_t keyboard_length;

extern char* prompt_string;
extern uint8_t prompt_length;

void kb_init(void) {
    current_character = kb_getc();
    last_character = current_character;
    display_address = device_get_hardware_address("display");
    
    if (display_address == 0) 
        return;
}

void kb_isr_callback(void) {
    current_character = kb_getc();
    if (last_character == current_character) 
        return;
    last_character = current_character;
    
    key_ready = 1;
}

uint8_t kb_check_input_state(void) {
    return key_ready;
}

uint8_t kb_get_current_char(void) {
    return current_character;
}

void kb_clear_input_state(void) {
    key_ready = 0;
}

void kb_event_handler(void) {
    if (key_ready == 0)
        return;
    key_ready = 0;
    
    interrupt_disable();
    
    char ch = current_character;
    
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
            console_process_command((char*)keyboard_string);
            
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
    
    if (keyboard_length < (KEYBOARD_STRING_LENGTH - 1)) {
        keyboard_string[keyboard_length] = ch;
        keyboard_length++;
        keyboard_string[keyboard_length] = '\0';
        
        char input[2];
        input[0] = ch;
        input[1] = '\0';
        
        print(input);
    }
}

void print(const char* str) {
    console_busy_wait();
    
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
            
            if (console_line < (display_get_height() - 1)) {
                console_line++;
            } else {
                uint8_t one = 1;
                mmio_writeb(&bus, display_address + 0x00005, &one);
                console_busy_wait();
            }
            
            continue;
        } else {
            
            if (console_get_position() >= display_get_width()) {
                console_position = 0;
                
                if (console_get_line() < (display_get_height() - 1)) {
                    console_line++;
                } else {
                    uint8_t one = 1;
                    mmio_writeb(&bus, display_address + 0x00005, &one);
                    console_busy_wait();
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

void print_prompt(void) {
    print(prompt_string);
}


typedef struct {
    uint8_t high;
    char ascii;
} KeyMap;

static const KeyMap map_DF[] = {{0x9A, 0x05}, {0x96, ']'}, {0x90, 'i'}, {0xC6, 's'}, {0x88, 'd'}, {0xCA, 'f'}, {0xCC, 'h'}, {0x8E, 'j'}, {0xD2, 'l'}};
static const KeyMap map_9F[] = {{0xC4, 0x12}, {0xD9, 0x01}, {0xD6, 0x02}, {0xDC, 0x04}, {0xD3, '-'}, {0x83, '`'}, {0x9D, 0x07}, {0x92, '/'}, {0x94, 0x27}, {0x91, '9'}, {0x8F, '8'}, {0xCD, '6'}, {0xCB, '5'}, {0x89, '3'}, {0xC7, '2'}, {0x85, '1'}, {0x86, 'z'}, {0xC8, 'x'}, {0x8A, 'v'}, {0x8C, 'b'}, {0xCE, 'm'}, {0xD0, 'k'}};
static const KeyMap map_5F[] = {{0xD6, 0x12}, {0x9D, 0x03}, {0x8A, 0x20}, {0xC4, 0x09}, {0xD5, '='}, {0xDC, 0x10}, {0xD0, ','}, {0x92, '.'}, {0x97, 0x5C}, {0x91, '0'}, {0x8F, '7'}, {0x89, '4'}, {0xC8, 'c'}, {0x8C, 'n'}, {0x85, 'q'}, {0xC7, 'w'}, {0xCB, 'r'}, {0xCD, 'y'}, {0xD3, 'p'}};
static const KeyMap map_1F[] = {{0xDD, 0x06}, {0xC5, 0x08}, {0x93, ';'}, {0x95, '['}, {0xC9, 'e'}, {0x8B, 't'}, {0xCF, 'u'}, {0xD1, 'o'}, {0x87, 'a'}, {0x8D, 'g'}};

char lookup(const KeyMap* table, size_t len, uint8_t high) {
    for (size_t i = 0; i < len; i++) {
        if (table[i].high == high) return table[i].ascii;
    }
    return 0;
}

char kb_getc(void) {
    struct Bus bus = {1, 2};
    uint8_t low, high;
    mmio_readb(&bus, 0x90000, &low);
    mmio_readb(&bus, 0x90001, &high);
    
    // Shift Logic
    if (high == 0xC4 && (low >= 0x90 && low <= 0x9B)) return 0x11; // Left Shift
    if (high == 0xD6 && (low >= 0x50 && low <= 0x5B)) return 0x12; // Right Shift
    
    switch (low) {
        case 0xDF: return lookup(map_DF, sizeof(map_DF), high);
        case 0x9F: return lookup(map_9F, sizeof(map_9F), high);
        case 0x5F: return lookup(map_5F, sizeof(map_5F), high);
        case 0x1F: return lookup(map_1F, sizeof(map_1F), high);
        default: return 0;
    }
}

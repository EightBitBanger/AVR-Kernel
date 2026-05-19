#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include <kernel/boot/avr/interrupt.h>

#include <kernel/bus/bus.h>
#include <kernel/arch/avr/io.h>

#include <kernel/console/virtual_key.h>
#include <kernel/console/display.h>
#include <kernel/console/keyboard.h>
#include <kernel/console/console.h>
#include <kernel/console/print.h>

static volatile char current_character  = 0x00;
static volatile char last_character     = 0x00;
static volatile uint8_t isr_key_ready   = 0;

#ifdef KERNEL_PLATFORM_AVR
static const char scancode_to_ascii_set2[] = {
    0, 0, 0, 0, 0, 0, 0, 0,              // 00-07
    0, 0, 0, 0, 0, 0, '`', 0,            // 08-0F
    0, 0, 0, 0, 0, 'q', '1', 0,          // 10-17
    0, 0, 'z', 's', 'a', 'w', '2', 0,    // 18-1F
    0, 'c', 'x', 'd', 'e', '4', '3', 0,  // 20-27
    0, ' ', 'v', 'f', 't', 'r', '5', 0,  // 28-2F
    0, 'n', 'b', 'h', 'g', 'y', '6', 0,  // 30-37
    0, 0, 'm', 'j', 'u', '7', '8', 0,    // 38-3F
    0, ',', 'k', 'i', 'o', '0', '9', 0,  // 40-47
    0, '.', '/', 'l', ';', 'p', '-', 0,  // 48-4F
    0, 0, '\'', 0, '[', '=', 0, 0,       // 50-57
    0, 0, 0x02, ']', 0, '\\', 0, 0,      // 58-5F  0x02 Enter
    0, 0, 0, 0, 0, 0, 0x01, 0            // 60-67  0x01 Backspace
};
#endif

void kb_init(void) {
    current_character = kb_getc();
    last_character = current_character;
}

void kb_isr_callback(void) {
    current_character = kb_getc();
    if (last_character == current_character) 
        return;
    last_character = current_character;
    
    isr_key_ready = 1;
}

uint8_t kb_check_input_state(void) {
    return isr_key_ready;
}

uint8_t kb_get_current_char(void) {
    return current_character;
}

void kb_clear_input_state(void) {
    isr_key_ready = 0;
}

extern char* keyboard_string;
extern uint8_t keyboard_length;
extern uint8_t keyboard_length_max;

void kb_event_handler(void) {
    if (isr_key_ready == 0)
        return;
    isr_key_ready = 0;
    
    if (display_get_device_address() == 0) 
        return;
    
    interrupt_disable();
    
    char ch = current_character;
    interrupt_enable();
    
    if (ch == 0x00)
        return;
    
    // Backspace
    if (ch == 0x01) {
        if (keyboard_length == 0) 
            return;
        
        uint8_t position = display_cursor_get_position();
        uint8_t line = display_cursor_get_line();
        
        if (position > 0) {
            position--;
        } else if (line > 0) {
            line--;
            position = display_get_width() - 1;
        } else {
            return;
        }
        keyboard_length--;
        
        display_cursor_set_position(position);
        display_cursor_set_line(line);
        display_putc(' ');
        return;
    }
    
    // Enter
    if (ch == 0x02) {
        keyboard_string[keyboard_length] = '\0';
        print("\n");
        
        if (keyboard_length > 0) {
            console_process_command(keyboard_string);
            
            if (display_cursor_get_position() != 0) 
                print("\n");
        }
        
        keyboard_length = 0;
        keyboard_string[0] = '\0';
        
        console_prompt_print();
        return;
    }
    
    if (ch < 0x20 || ch == 0x7f)
        return;
    
    if (keyboard_length < (keyboard_length_max - 1)) {
        keyboard_string[keyboard_length] = ch;
        keyboard_length++;
        keyboard_string[keyboard_length] = '\0';
        
        char input[2];
        input[0] = ch;
        input[1] = '\0';
        
        /*
        if (kb_vkey_check(VK_SHIFT_LEFT) || kb_vkey_check(VK_SHIFT_RIGHT)) {
            if (islower(input[0])) 
                input[0] = toupper(input[0]);
        }
        */
        print(input);
    }
}

void kb_get_raw(uint8_t* low_byte, uint8_t* high_byte) {
    struct Bus bus;
    bus.read_waitstate  = 4; 
    bus.write_waitstate = 1;
    
    _delay_us(80); 
    
    mmio_readb(&bus, 0x90000, low_byte);
    mmio_readb(&bus, 0x90001, high_byte);
}

char kb_getc(void) {
    uint8_t low_byte, high_byte;
    kb_get_raw(&low_byte, &high_byte);
    
    uint32_t scancode = 0;
    bool is_break = true;
    
    uint32_t extended = 0xF0;
    if ((high_byte & 0x80) && !(low_byte & 0x20) && (low_byte & 0x10)) {
        if (high_byte & 0x20) scancode |= 0x80;
        if (high_byte & 0x10) scancode |= 0x40;
        if (high_byte & 0x08) scancode |= 0x20;
        if (high_byte & 0x04) scancode |= 0x10;
        if (high_byte & 0x02) scancode |= 0x08;
        if (high_byte & 0x01) scancode |= 0x04;
        if (low_byte & 0x80)  scancode |= 0x02;
        if (low_byte & 0x40)  scancode |= 0x01;
        
        if (!(low_byte & 0x08)) {extended &= ~0x40; is_break = false;} 
        if (!(low_byte & 0x04)) {extended &= ~0x20; is_break = false;}
        if (!(low_byte & 0x02)) {extended &= ~0x10; is_break = false;}
        if (!(low_byte & 0x01)) {extended &= ~0x08; is_break = false;}
    }
    
    kb_vkey_set(scancode, !is_break);
    
    if (scancode < sizeof(scancode_to_ascii_set2) && !is_break) {
        return scancode_to_ascii_set2[scancode];
    }
    
    return 0;
}

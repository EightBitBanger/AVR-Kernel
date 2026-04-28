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

static volatile char    current_character = 0x00;
static volatile char    last_character    = 0x00;
static volatile uint8_t isr_key_ready     = 0;

//bool KeyTriggerLock[5];


static const char scancode_to_ascii[] = {
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
    0, 0, 0x02, ']', 0, '\\', 0, 0,      // 58-5F (0x02 is Enter)
    0, 0, 0, 0, 0, 0, 0x01, 0            // 60-67 (0x01 is Backspace)
};


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




// Get this outta heeere.!.!.!

extern char* keyboard_string;
extern uint8_t keyboard_length;




void kb_event_handler(void) {
    if (isr_key_ready == 0)
        return;
    isr_key_ready = 0;
    
    //uint8_t keyboard_length = console_get_keyboard_string_length();
    //uint8_t* keyboard_string;
    //console_get_keyboard_string(keyboard_string);
    
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
            // Normal backspace on the same line
            position--;
        } else if (line > 0) {
            // Move to the end of the previous line
            line--;
            position = display_get_width() - 1;
        } else {
            // No more backspacing
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
            console_process_command((char*)keyboard_string);
            
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
    
    if (keyboard_length < (KEYBOARD_STRING_LENGTH - 1)) {
        keyboard_string[keyboard_length] = ch;
        keyboard_length++;
        keyboard_string[keyboard_length] = '\0';
        
        char input[2];
        input[0] = ch;
        input[1] = '\0';
        
        // Shift case & caps lock
        if (kb_vkey_check(VK_SHIFT_LEFT) || kb_vkey_check(VK_SHIFT_RIGHT)) {
            if (islower(input[0])) 
                input[0] = toupper(input[0]);
            //if (!kb_vkey_check(VK_CAPSLOCK)) 
            //    if (islower(input[0])) 
            //        input[0] = toupper(input[0]);
        } /* else if (kb_vkey_check(VK_CAPSLOCK)) {
            if (islower(input[0])) 
                input[0] = toupper(input[0]);
        }
        */
        print(input);
    }
}

void kb_get_raw(uint8_t* low_byte, uint8_t* high_byte) {
    struct Bus bus;
    bus.read_waitstate  = 700;
    bus.write_waitstate = 1;
    
    mmio_readb(&bus, 0x90000, low_byte);
    mmio_readb(&bus, 0x90001, high_byte);
}

char kb_getc(void) {
    uint8_t low_byte, high_byte;
    kb_get_raw(&low_byte, &high_byte);
    
    uint32_t scancode = 0;
    uint32_t extended = 0xF0;
    bool is_break = true;
    
    // Check start & stop bits
    if ((high_byte & 0x80) && !(low_byte & 0x20) && (low_byte & 0x10)) {
        //if (high_byte & 0x40) // Parity bit
        if (high_byte & 0x20) scancode |= 0x80;
        if (high_byte & 0x10) scancode |= 0x40;
        if (high_byte & 0x08) scancode |= 0x20;
        if (high_byte & 0x04) scancode |= 0x10;
        if (high_byte & 0x02) scancode |= 0x08;
        if (high_byte & 0x01) scancode |= 0x04;
        if (low_byte & 0x80)  scancode |= 0x02;
        if (low_byte & 0x40)  scancode |= 0x01;
        
        if (!(low_byte & 0x08)) {extended &= ~0x40; is_break = false;} // Parity bit
        if (!(low_byte & 0x04)) {extended &= ~0x20; is_break = false;}
        if (!(low_byte & 0x02)) {extended &= ~0x10; is_break = false;}
        if (!(low_byte & 0x01)) {extended &= ~0x08; is_break = false;}
    }
    
    // Special key states
    if (!is_break) { // Pressed
        
        //if (scancode == 0x58) {if (!KeyTriggerLock[0]) kb_vkey_set(VK_CAPSLOCK, !kb_vkey_check(VK_CAPSLOCK)); KeyTriggerLock[0] = true; return 0;}
        //if (scancode == 0x70) {if (!KeyTriggerLock[1]) kb_vkey_set(VK_INSERT,   !kb_vkey_check(VK_INSERT));   KeyTriggerLock[1] = true; return 0;}
        
        kb_vkey_set(scancode, 1);
    } else { // Released
        
        //if (scancode == 0x58) {KeyTriggerLock[0] = false; return 0;}   // Caps lock
        //if (scancode == 0x70) {KeyTriggerLock[1] = false; return 0;}   // Insert
        
        kb_vkey_set(scancode, 0);
    }
    
    // Translate to ASCII
    if (scancode < sizeof(scancode_to_ascii) && is_break == 0) {
        return scancode_to_ascii[scancode];
    }
    
    return 0;
}

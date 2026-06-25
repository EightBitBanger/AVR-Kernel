#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include <kernel/arch/x86/io.h>

#include <kernel/console/virtual_key.h>
#include <kernel/console/display.h>
#include <kernel/console/keyboard.h>
#include <kernel/console/console.h>
#include <kernel/console/print.h>
#include <kernel/util/system.h>

static volatile char current_character  = 0x00;
static volatile char last_character     = 0x00;
static volatile uint8_t isr_key_ready   = 0;

static bool is_ctrl_pressed = false;
static bool is_alt_pressed  = false;
static bool is_shift_pressed = false;

// Native PS/2 Scan Code Set 1 Table
static const char scancode_to_ascii_set1[] = {
    0,  0, '1', '2', '3', '4', '5', '6', '7', '8',     /* 0x00 - 0x09 */
  '9', '0', '-', '=', 0x01, '\t', 'q', 'w', 'e', 'r',  /* 0x0A - 0x13 (0x01 is Backspace) */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0x02,   0,   /* 0x14 - 0x1D (0x02 is Enter) */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',    /* 0x1E - 0x27 */
 '\'', '`',   0, '\\', 'z', 'x', 'c', 'v', 'b', 'n',   /* 0x28 - 0x31 */
  'm', ',', '.', '/',   0, '*',   0, ' ',   0,   0,    /* 0x32 - 0x3B */
};

static const char scancode_to_ascii_shifted_set1[] = {
    0,  0, '!', '@', '#', '$', '%', '^', '&', '*',     /* 0x00 - 0x09 */
  '(', ')', '_', '+', 0x01, '\t', 'Q', 'W', 'E', 'R',  /* 0x0A - 0x13 */
  'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0x02,   0,   /* 0x14 - 0x1D */
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',    /* 0x1E - 0x27 */
 '"', '~',   0, '|', 'Z', 'X', 'C', 'V', 'B', 'N',   /* 0x28 - 0x31 */
  'M', '<', '>', '?',   0, '*',   0, ' ',   0,   0,    /* 0x32 - 0x3B */
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

extern char* keyboard_string;
extern uint8_t keyboard_length;
extern uint8_t keyboard_length_max;

void kb_event_handler(void) {
    current_character = kb_getc();
    
    if (current_character == 0x00) {
        last_character = 0x00; 
        return;
    }
    
    if (current_character == last_character) 
        return;
    
    last_character = current_character;
    char ch = current_character;
    
    // Backspace
    if (ch == 0x01) {
        if (keyboard_length == 0) return;
        
        uint8_t position = display_cursor_get_position();
        uint8_t line = display_cursor_get_line();
        
        if (position > 0) {
            position--;
        } else if (line > 0) {
            line--;
            position = display_get_rows() - 1;
        } else {
            return;
        }
        
        keyboard_length--;
        keyboard_string[keyboard_length] = '\0';
        
        display_cursor_set_position(position);
        display_cursor_set_line(line);
        display_putc(' '); 
        display_cursor_set_position(position); 
        display_cursor_set_line(line);
        return;
    }
    
    // Enter
    if (ch == 0x02) {
        print("\n");
        if (keyboard_length > 0) {
            console_process_command(keyboard_string);
            if (display_cursor_get_position() != 0) print("\n");
        }
        keyboard_length = 0;
        keyboard_string[0] = '\0';
        console_prompt_print();
        return;
    }
    
    // Print printable characters
    if (ch < 0x20 || ch == 0x7f) 
        return;
    
    if (keyboard_length < keyboard_length_max - 1) { 
        keyboard_string[keyboard_length] = ch;
        keyboard_length++;
        keyboard_string[keyboard_length] = '\0';
        
        char input[2] = {ch, '\0'};
        print(input);
    }
}

void kb_get_raw(uint8_t* low_byte, uint8_t* high_byte) {
}

uint16_t kb_getc(void) {
    // Check PS/2 Controller Status Register (Port 0x64)
    // Bit 0 (0x01) is set when the Output Buffer has a scan code waiting to be read.
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) {
        return 0; // No key data available right now
    }
    
    // Check if this data came from the mouse instead of the keyboard (Bit 5 / 0x20)
    if (status & 0x20) {
        inb(0x60); // Flush the mouse byte to avoid locking up the port
        return 0;
    }

    // Read the primary scan code byte from the PS/2 Data Port (Port 0x60)
    uint8_t scancode = inb(0x60);
    bool is_extended = false;

    // --- HANDLE EXTENDED SCROLL SCAN CODES (0xE0 Prefix) ---
    if (scancode == 0xE0) {
        // Spin briefly until the second byte of the sequence lands in the buffer
        int timeout = 20000;
        while (!((inb(0x64) & 0x01)) && timeout > 0) {
            timeout--;
        }
        
        // If the second byte arrived, grab it; otherwise drop the trailing prefix safely
        if (timeout > 0) {
            scancode = inb(0x60);
            is_extended = true;
        } else {
            return 0; 
        }
    }

    // Determine if this is a Make code (press) or Break code (release)
    // In Set 1, a break code has bit 7 (0x80) enabled.
    bool is_break = (scancode & 0x80) ? true : false;
    scancode &= 0x7F; // Strip bit 7 to isolate the core scancode identifier

    // --- STATE TRACKING FOR MODIFIER KEYS ---
    if (scancode == 0x1D) {         // Left Ctrl
        is_ctrl_pressed = !is_break;
    } else if (scancode == 0x38) {  // Left Alt
        is_alt_pressed = !is_break;
    } else if (scancode == 0x2A || scancode == 0x36) { // Left/Right Shift
        is_shift_pressed = !is_break;
    }

    // Handle Ctrl+Alt+Del Reset Sequence
    if (scancode == 0x53 && !is_break) {
        if (is_ctrl_pressed && is_alt_pressed) {
            system_restart();
        }
    }

    // Forward the key state updates down to virtual tracking subsystems
    kb_vkey_set(scancode, !is_break);

    // We only process 'Make' events (key presses) for text area character insertion
    if (!is_break) {
        // Handle Directional Arrow Keys
        // Arrow key scan codes are identical in make values, but carry the extended 0xE0 flag.
        if (is_extended) {
            if (scancode == 0x48 || scancode == 0x4B || scancode == 0x4D || scancode == 0x50) {
                return (uint16_t)(scancode << 8); // Pack raw scancode directly into the high byte
            }
            return 0; // Skip other non-navigation extended keys for now
        }

        // Handle standard printable ASCII scan code translations
        if (scancode < sizeof(scancode_to_ascii_set1)) {
            if (is_shift_pressed) {
                return (uint16_t)scancode_to_ascii_shifted_set1[scancode];
            } else {
                return (uint16_t)scancode_to_ascii_set1[scancode];
            }
        }
    }

    return 0;
}

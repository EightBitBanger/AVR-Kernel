#include <drivers/keyboard/ps2/main.h>

#include <kernel/kernel.h>

struct PS2DeviceDriver keyboardDeviceDriver;

struct PS2DeviceDriver* keyboardDriver = &keyboardDeviceDriver;



// Driver function declarations

void __read_ps2_device(uint32_t address, uint8_t* buffer);
void __write_ps2_device(uint32_t address, uint8_t buffer);

uint8_t decodeScanCode(uint8_t scancode_low, uint8_t scancode_high);


void initiatePS2Driver(void) {
    
    uint8_t deviceName[] = "PS2";
    
    for (unsigned int i=0; i < sizeof(deviceName); i++) 
        keyboardDriver->device.device_name[i] = deviceName[i];
    
    // Register the driver with the kernel
	RegisterDriver( (void*)keyboardDriver );
	
	// Set hardware device details
    
#ifdef BOARD_RETROBOARD_REV2
    keyboardDriver->device.hardware_address = 0x90000;
#endif
    
#ifdef BOARD_RETRO_AVR_X4_REV1
    keyboardDriver->device.hardware_address = 0x00002;
#endif
	
	keyboardDriver->device.device_id = 0x01;
	
	keyboardDriver->device.bus.read_waitstate  = 2;
	keyboardDriver->device.bus.write_waitstate = 2;
    
    
    keyboardDriver->read  = __read_ps2_device;
    keyboardDriver->write = __write_ps2_device;
    
    keyboardDriver->is_linked = 0;
    
    return;
}


void __read_ps2_device(uint32_t address, uint8_t* buffer) {
    bus_read_byte( &keyboardDriver->device.bus, keyboardDriver->device.hardware_address + address, buffer );
    return;
}

void __write_ps2_device(uint32_t address, uint8_t buffer) {
    bus_write_byte( &keyboardDriver->device.bus, keyboardDriver->device.hardware_address + address, buffer );
    return;
}


// Decode the scan code returning an ASCII character
uint8_t kbDecodeScanCode(uint8_t scancode_low, uint8_t scancode_high) {
    
#ifdef BOARD_RETRO_AVR_X4_REV1
    
	if ((scancode_low == 0x19) | 
        (scancode_low == 0x99) | 
        (scancode_low == 0x49) | 
        (scancode_low == 0x89) | 
        (scancode_low == 0x09) | 
        (scancode_low == 0x59) | 
        (scancode_low == 0xC9) | 
        (scancode_low == 0x09) | 
        (scancode_low == 0xD9)) {
		if (scancode_high == 0x23) {return 0x11;} // Left shift pressed
	}
	
	if ((scancode_low == 0x1A) | 
        (scancode_low == 0x9A) | 
        (scancode_low == 0x4A) | 
        (scancode_low == 0x8A) | 
        (scancode_low == 0x0A) | 
        (scancode_low == 0x5A) | 
        (scancode_low == 0xCA) | 
        (scancode_low == 0xDA)) {
		if (scancode_high == 0x6b) {return 0x11;} // Right shift pressed
	}
	
	if (scancode_low == 0xFB) {
		if (scancode_high == 0x59) {return 0x05;} // Left arrow
		if (scancode_high == 0x69) {return ']';}  // Right square bracket
		if (scancode_high == 0x03) {return 0xf5;} // Function F5
		if (scancode_high == 0x41) {return 0xf6;} // Function F6
		if (scancode_high == 0x05) {return 0xf7;} // Function F7
		if (scancode_high == 0x81) {return 0xfc;} // Function F12
		if (scancode_high == 0x09) {return 'i';}
		if (scancode_high == 0x63) {return 's';}
		if (scancode_high == 0x11) {return 'd';}
		if (scancode_high == 0x53) {return 'f';}
		if (scancode_high == 0x33) {return 'h';}
		if (scancode_high == 0x71) {return 'j';}
		if (scancode_high == 0x4B) {return 'l';}
		return  0x00;
	}
	
	if (scancode_low == 0xF9) {
		if (scancode_high == 0x23) {return 0x12;} // Shift left released
		if (scancode_high == 0x9B) {return 0x01;} // Backspace
		if (scancode_high == 0x6B) {return 0x02;} // Enter
		if (scancode_high == 0x3B) {return 0x04;} // Down arrow
		if (scancode_high == 0xCB) {return '-';}  // Dash
		if (scancode_high == 0xC1) {return '`';}  // Apostrophe
		if (scancode_high == 0xB9) {return 0x07;} // Escape
		if (scancode_high == 0x49) {return '/';}  // Forward slash
		if (scancode_high == 0x29) {return 0x27;} // '
		if (scancode_high == 0x83) {return 0xf2;} // Function F2
		if (scancode_high == 0x43) {return 0xf8;} // Function F8
		if (scancode_high == 0x89) {return '9';}
		if (scancode_high == 0xF1) {return '8';}
		if (scancode_high == 0xB3) {return '6';}
		if (scancode_high == 0xD3) {return '5';}
		if (scancode_high == 0x91) {return '3';}
		if (scancode_high == 0xE3) {return '2';}
		if (scancode_high == 0xA1) {return '1';}
		if (scancode_high == 0x61) {return 'z';}
		if (scancode_high == 0x13) {return 'x';}
		if (scancode_high == 0x51) {return 'v';}
		if (scancode_high == 0x31) {return 'b';}
		if (scancode_high == 0x73) {return 'm';}
		if (scancode_high == 0x0b) {return 'k';}
		return  0x00;
	}
	
	if (scancode_low == 0xFA) {
		if (scancode_high == 0x6B) {return 0x12;} // Right shift released
		if (scancode_high == 0xB9) {return 0x03;} // Up arrow
		if (scancode_high == 0x51) {return 0x20;} // Space
		if (scancode_high == 0x23) {return 0x09;} // Alt
		if (scancode_high == 0xAB) {return '=';}  // Equals
		if (scancode_high == 0x3B) {return 0x10;} // Delete
		if (scancode_high == 0x0b) {return ',';}  // Comma
		if (scancode_high == 0x49) {return '.';}  // Period
		if (scancode_high == 0xE9) {return 0x5c;} // Backslash
		if (scancode_high == 0x83) {return 0xf1;} // Function F1
		if (scancode_high == 0x01) {return 0xf9;} // Function F9
		if (scancode_high == 0x43) {return 0xfa;} // Function F10
		if (scancode_high == 0x89) {return '0';}
		if (scancode_high == 0xF1) {return '7';}
		if (scancode_high == 0x91) {return '4';}
		if (scancode_high == 0x13) {return 'c';}
		if (scancode_high == 0x31) {return 'n';}
		if (scancode_high == 0xA1) {return 'q';}
		if (scancode_high == 0xE3) {return 'w';}
		if (scancode_high == 0xD3) {return 'r';}
		if (scancode_high == 0xB3) {return 'y';}
		if (scancode_high == 0xCB) {return 'p';}
		return  0x00;
	}
	
	if (scancode_low == 0xf8) {
		if (scancode_high == 0xBB) {return 0x06;} // Right arrow
		if (scancode_high == 0xA3) {return 0x08;} // Control
		if (scancode_high == 0xC9) {return ';';}  // Semi-colon
		if (scancode_high == 0xA9) {return '[';}  // Left square bracket
		if (scancode_high == 0x81) {return 0xf3;} // Function F3
		if (scancode_high == 0xc3) {return 0xf4;} // Function F4
		if (scancode_high == 0xb7) {return 0xfb;} // Function F11
		if (scancode_high == 0x93) {return 'e';}
		if (scancode_high == 0xD1) {return 't';}
		if (scancode_high == 0xF3) {return 'u';}
		if (scancode_high == 0x8B) {return 'o';}
		if (scancode_high == 0xE1) {return 'a';}
		if (scancode_high == 0xB1) {return 'g';}
		return  0x00;
	}
	
#endif
    
#ifdef BOARD_RETROBOARD_REV2
    
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
	
#endif
    
	return 0x00;
}

void kbPrintInput(void) {
    uint8_t nameKeyboard[] = "PS2";
	struct Driver* keyboadDevice = (struct Driver*)GetDriverByName( nameKeyboard, sizeof(nameKeyboard) );
	
	uint8_t oldScanCodeLow  = 0;
    uint8_t oldScanCodeHigh = 0;
    
    uint8_t currentScanCodeLow  = 0;
    uint8_t currentScanCodeHigh = 0;
    
    while(1) {
        
        uint8_t hexStringLow[2];
        uint8_t hexStringHigh[2];
        
        
#ifdef BOARD_RETRO_AVR_X4_REV1
        keyboadDevice->read( 0x00001, &currentScanCodeLow );
        keyboadDevice->read( 0x00000, &currentScanCodeHigh );
#endif
        
#ifdef BOARD_RETROBOARD_REV2
        keyboadDevice->read( 0x00000, &currentScanCodeLow );
        keyboadDevice->read( 0x00001, &currentScanCodeHigh );
#endif
        
        if ((oldScanCodeLow  == currentScanCodeLow) & 
            (oldScanCodeHigh == currentScanCodeHigh)) 
            continue;
        
        oldScanCodeLow  = currentScanCodeLow;
        oldScanCodeHigh = currentScanCodeHigh;
        
        int_to_hex_string(oldScanCodeLow,  hexStringLow);
        int_to_hex_string(oldScanCodeHigh, hexStringHigh);
        
        print(hexStringLow, sizeof(hexStringLow)+1);
        printChar(' ');
        print(hexStringHigh, sizeof(hexStringHigh)+1);
        printLn();
        
        continue;
    }
    
    return;
}



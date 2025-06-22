#include <avr/io.h>

#include <kernel/delay.h>

#include <kernel/command/cli.h>

#include <drivers/keyboard/ps2/main.h>

extern uint32_t fs_working_directory_address;

uint16_t display_width  = 0;
uint16_t display_height = 0;

uint8_t console_string[CONSOLE_STRING_LENGTH];
uint8_t console_string_old[CONSOLE_STRING_LENGTH];
uint8_t console_string_length     = 0;
uint8_t console_string_length_old = 0;

uint8_t parameters_begin;

uint8_t console_position = 0;
uint8_t console_line     = 0;

uint8_t console_width   = 21;
uint8_t console_height  = 8;

uint8_t oldScanCodeLow  = 0;
uint8_t oldScanCodeHigh = 0;

uint8_t lastChar = 0x00;

uint8_t shiftState = 0;

uint8_t console_prompt[16];
uint8_t console_prompt_length = 1;

uint8_t cursor_blink_rate = CURSOR_BLINK_RATE;

uint16_t displayRows    = 8;
uint16_t displayColumbs = 21;

struct Driver* displayDevice;
struct Driver* keyboadDevice;


struct ConsoleCommand CommandRegistry[CONSOLE_FUNCTION_TABLE_SIZE];


uint8_t ConsoleRegisterCommand(uint8_t* name, uint8_t nameLength, void(*functionPtr)(uint8_t* string, uint8_t length)) {
    
    uint8_t index = 0;
    for (uint8_t i=0; i < CONSOLE_FUNCTION_TABLE_SIZE; i++) {
        
        // Check valid value
        if ((is_letter( &CommandRegistry[i].name[0] ) == 1) | 
            (is_number( &CommandRegistry[i].name[0] ) == 1))
            continue;
        
        index = i;
        
        break;
    }
    
    if (nameLength > CONSOLE_FUNCTION_NAME_LENGTH) 
        nameLength = CONSOLE_FUNCTION_NAME_LENGTH;
    
    for (uint8_t i=0; i < nameLength-1; i++) 
        CommandRegistry[index].name[i] = name[i];
    
    CommandRegistry[index].function = functionPtr;
	
	return 1;
}

void cliInit(void) {
    
    uint8_t nameKeyboard[] = "PS2";
	keyboadDevice = (struct Driver*)GetDriverByName( nameKeyboard, sizeof(nameKeyboard) );
	
	uint8_t nameDisplay[] = "display";
	displayDevice = (struct Driver*)GetDriverByName( nameDisplay, sizeof(nameDisplay) );
	
#ifdef BOARD_RETRO_AVR_X4_REV1
    keyboadDevice->read( 0x00001, &oldScanCodeLow );
    keyboadDevice->read( 0x00000, &oldScanCodeHigh );
#endif
    
#ifdef BOARD_RETROBOARD_REV2
    keyboadDevice->read( 0x00000, &oldScanCodeLow );
    keyboadDevice->read( 0x00001, &oldScanCodeHigh );
#endif
    
    lastChar = kbDecodeScanCode(oldScanCodeLow, oldScanCodeHigh);
    
    for (uint8_t i=0; i < CONSOLE_STRING_LENGTH; i++) {
        console_string[i] = ' ';
    }
    
	for (uint8_t i=0; i < CONSOLE_FUNCTION_TABLE_SIZE; i++) {
        
        for (uint8_t n=0; n < CONSOLE_FUNCTION_NAME_LENGTH; n++) 
            CommandRegistry[i].name[n] = ' ';
        
        CommandRegistry[i].function = nullptr;
	}
	
	uint8_t promptDeviceString[] = "A>";
	
	displayDevice->write(3, 60);  // Cursor blink rate
    displayDevice->write(4, '_'); // Cursor character
    
    ConsoleSetPrompt( promptDeviceString, sizeof(promptDeviceString) );
	
	return;
}

uint8_t ConsoleGetRawChar(void) {
    
    uint8_t scanCodeLow  = 0;
    uint8_t scanCodeHigh = 0;
    
#ifdef BOARD_RETRO_AVR_X4_REV1
    keyboadDevice->read( 0x00001, &scanCodeLow );
    keyboadDevice->read( 0x00000, &scanCodeHigh );
#endif
    
#ifdef BOARD_RETROBOARD_REV2
    keyboadDevice->read( 0x00000, &scanCodeLow );
    keyboadDevice->read( 0x00001, &scanCodeHigh );
#endif
    
    return kbDecodeScanCode(scanCodeLow, scanCodeHigh);
}

uint8_t ConsoleGetLastChar(void) {
    
    uint8_t scanCodeLow  = 0;
    uint8_t scanCodeHigh = 0;
    
#ifdef BOARD_RETRO_AVR_X4_REV1
    keyboadDevice->read( 0x00001, &scanCodeLow );
    keyboadDevice->read( 0x00000, &scanCodeHigh );
#endif
    
#ifdef BOARD_RETROBOARD_REV2
    keyboadDevice->read( 0x00000, &scanCodeLow );
    keyboadDevice->read( 0x00001, &scanCodeHigh );
#endif
    
    if ((oldScanCodeLow == scanCodeLow) & 
        (oldScanCodeHigh == scanCodeHigh)) 
        return 0;
    
    oldScanCodeLow  = scanCodeLow;
    oldScanCodeHigh = scanCodeHigh;
    
    uint8_t scanCode = kbDecodeScanCode(scanCodeLow, scanCodeHigh);
    
    if (scanCode == 0) 
        return 0;
    
    lastChar = scanCode;
    
    return lastChar;
}

void ConsoleClearKeyboardString(void) {
    
    for (uint8_t i=0; i < CONSOLE_STRING_LENGTH; i++) 
        console_string[i] = ' ';
    
    return;
}

void ConsolePrintKeyboardString(void) {
    
    ConsoleSetCursorPosition(console_prompt_length-1);
    
    for (uint8_t i=0; i < console_string_length; i++) 
        printChar( console_string[i] );
    
    
    ConsoleSetCursorPosition( (console_prompt_length + console_string_length) - 1 );
    
    if (console_position >= displayColumbs) {
        
        printLn();
        
        console_position = 0;
        ConsoleSetCursorPosition(console_position);
        
    }
    
    return;
}

uint8_t ConsoleWait(void) {
    
#ifdef BOARD_RETRO_AVR_X4_REV1
    keyboadDevice->read( 0x00001, &oldScanCodeLow );
    keyboadDevice->read( 0x00000, &oldScanCodeHigh );
#endif
    
#ifdef BOARD_RETROBOARD_REV2
    keyboadDevice->read( 0x00000, &oldScanCodeLow );
    keyboadDevice->read( 0x00001, &oldScanCodeHigh );
#endif
    
    uint8_t currentChar = kbDecodeScanCode(oldScanCodeLow, oldScanCodeHigh);
    currentChar = lastChar;
    
    while (currentChar == lastChar) {
        
#ifdef BOARD_RETRO_AVR_X4_REV1
        keyboadDevice->read( 0x00001, &oldScanCodeLow );
        keyboadDevice->read( 0x00000, &oldScanCodeHigh );
#endif
        
#ifdef BOARD_RETROBOARD_REV2
        keyboadDevice->read( 0x00000, &oldScanCodeLow );
        keyboadDevice->read( 0x00001, &oldScanCodeHigh );
#endif
        
        currentChar = kbDecodeScanCode(oldScanCodeLow, oldScanCodeHigh);
        
        if (currentChar == 0x00) 
            lastChar = currentChar;
        
        continue;
    }
    
    lastChar = currentChar;
    
    return lastChar;
}


//
// Console functionality
//

void ConsoleSetBlinkRate(uint8_t rate) {
    
    cursor_blink_rate = rate;
    
    __glBusyWait();
    displayDevice->write( 3, rate);
    
    return;
}

void ConsoleSetCursor(uint8_t line, uint8_t position) {
    
    console_line = line;
    
    console_position = position;
    
    __glBusyWait();
    displayDevice->write( 1, console_line );
    displayDevice->write( 2, console_position );
    
    return;
}

void ConsoleSetCursorPosition(uint8_t position) {
    
    console_position = position;
    
    __glBusyWait();
    displayDevice->write( 2, console_position );
    return;
}

void ConsoleCursorEnable(void) {
    __glBusyWait();
    displayDevice->write( 3, cursor_blink_rate);
    return;
}

void ConsoleCursorDisable(void) {
    __glBusyWait();
    displayDevice->write( 3, 0);
    return;
}

void ConsoleSetPrompt(uint8_t* prompt, uint8_t length) {
    
    for (uint8_t i=0; i < length; i++) 
        console_prompt[i] = prompt[i];
    
    console_prompt_length = length;
    return;
}

void ConsoleClearScreen(uint8_t clearToCharacter) {
    __glBusyWait();
    for (uint8_t l=0; l < 8; l++) {
        
        displayDevice->write(0x00001, l);
        displayDevice->write(0x00002, 0);
        
        for (uint8_t c=0; c < 21; c++) 
            displayDevice->write( 0x00010 + c, clearToCharacter );
        
    }
    
    swapBuffers();
    return;
}

uint8_t ConsoleGetCursorPosition(void) {
    return console_position;
}

uint8_t ConsoleGetCursorLine(void) {
    return console_line;
}

void ConsoleSetDisplayWidth(uint16_t width) {
    console_width = width;
    return;
}

void ConsoleSetDisplayHeight(uint16_t height) {
    console_height = height;
    return;
}


uint16_t ConsoleGetDisplayWidth(void) {
    return console_width;
}

uint16_t ConsoleGetDisplayHeight(void) {
    return console_height;
}


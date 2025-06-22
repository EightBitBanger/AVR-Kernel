#include <avr/io.h>

#include <kernel/delay.h>

#include <kernel/command/cli.h>

extern uint32_t fs_working_directory_address;

extern uint16_t display_width;
extern uint16_t display_height;

extern uint8_t console_string[CONSOLE_STRING_LENGTH];
extern uint8_t console_string_old[CONSOLE_STRING_LENGTH];
extern uint8_t console_string_length;
extern uint8_t console_string_length_old;

extern uint8_t console_position;
extern uint8_t console_line;

extern uint8_t oldScanCodeLow;
extern uint8_t oldScanCodeHigh;

extern uint8_t lastChar;

extern uint8_t shiftState;

extern uint8_t console_prompt[32];
extern uint8_t console_prompt_length;

extern uint8_t cursor_blink_rate;

extern uint16_t displayRows;
extern uint16_t displayColumbs;

extern struct Driver* displayDevice;
extern struct Driver* keyboadDevice;

extern struct ConsoleCommand CommandRegistry[CONSOLE_FUNCTION_TABLE_SIZE];

void KeyFunctionReturn(void);
void KeyFunctionBackspace(void);
void KeyFunctionPrintChar(uint8_t scanCode);


uint8_t cliRunShell(void) {
    
    // Check the current scan code
    uint8_t scanCode = ConsoleGetLastChar();
    
    if (scanCode == 0) 
        return 0;
    
    // Shift pressed / released
    if (scanCode == 0x11) shiftState = 1;
    if (scanCode == 0x12) shiftState = 0;
    
    if (scanCode < 0x20) {
        
        if (scanCode == 0x01) {KeyFunctionBackspace();}
        if (scanCode == 0x02) {KeyFunctionReturn();}
        
    } else {
        
        KeyFunctionPrintChar(scanCode);
        
    }
    
    return 1;
}



void KeyFunctionPrintChar(uint8_t scanCode) {
    if (console_string_length >= CONSOLE_STRING_LENGTH) 
        return;
    
    if (shiftState == 1) {
        
        if (is_number(&scanCode) == 1) {
            
            if (scanCode == '0') scanCode = ')';
            if (scanCode == '1') scanCode = '!';
            if (scanCode == '2') scanCode = '@';
            if (scanCode == '3') scanCode = '#';
            if (scanCode == '4') scanCode = '$';
            if (scanCode == '5') scanCode = '%';
            if (scanCode == '6') scanCode = '^';
            if (scanCode == '7') scanCode = '&';
            if (scanCode == '8') scanCode = '*';
            if (scanCode == '9') scanCode = '(';
            
        }
        
        if (is_letter(&scanCode) == 0) {
            
            if ((scanCode == '[') | (scanCode == ']')) scanCode += 0x20;
            
            if (scanCode == 0x3B) scanCode--;
            
            if (scanCode == '-')  scanCode = '_';
            if (scanCode == '=')  scanCode = '+';
            if (scanCode == '\\') scanCode = '|';
            if (scanCode == 0x27) scanCode = 0x22;
            if (scanCode == ',')  scanCode = '<';
            if (scanCode == '.')  scanCode = '>';
            if (scanCode == '/')  scanCode = '?';
            
        } else {
            
            scanCode -= 0x20;
        }
        
    }
    
    console_string[console_string_length] = scanCode;
    console_string_length++;
    
    printChar(scanCode);
    
    ConsoleSetCursorPosition(console_position);
    
    // New line
    if (console_position >= displayColumbs) {
        
        printLn();
        
        console_position = 0;
        ConsoleSetCursorPosition(console_position);
    }
    
    return;
}



void KeyFunctionBackspace(void) {
    if (console_string_length == 0) 
        return;
    
    // Remove last character from the console string
    console_string[ console_string_length - 1 ] = ' ';
    
    // Decrement the console string length
    if (console_position == 0) {
        
        console_line--;
        console_position=21;
        
        displayDevice->write( 0x00001, console_line);
        
    }
    
    console_string_length--;
    console_position--;
    
    displayDevice->write( 0x00002, console_position);
    displayDevice->write( 0x00010, ' ');
    displayDevice->write( 0x00002, console_position);
    
    swapBuffers();
    return;
}



#ifndef __CONSOLE_COMMAND_LINE_INTERPRETER_
#define __CONSOLE_COMMAND_LINE_INTERPRETER_

#define CURSOR_BLINK_RATE  60
#define CONSOLE_STRING_LENGTH  40
#define CONSOLE_FUNCTION_NAME_LENGTH   10

#define CONSOLE_FUNCTION_TABLE_SIZE  8

#include <kernel/kernel.h>

struct ConsoleCommand {
    
    uint8_t name[CONSOLE_FUNCTION_NAME_LENGTH];
    
    void(*function)(uint8_t* string, uint8_t length);
    
};


uint8_t cliRunShell(void);

void cliInit(void);

uint8_t ConsoleRegisterCommand(uint8_t* name, uint8_t nameLength, void(*functionPtr)(uint8_t* string, uint8_t length));

uint8_t ConsoleGetRawChar(void);
uint8_t ConsoleGetLastChar(void);

void ConsoleSetCursor(uint8_t line, uint8_t position);
void ConsoleCursorEnable(void);
void ConsoleCursorDisable(void);
void ConsoleSetCursorPosition(uint8_t position);
void ConsoleSetBlinkRate(uint8_t rate);
void ConsoleSetPrompt(uint8_t* prompt, uint8_t length);
void ConsoleClearScreen(uint8_t clearToCharacter);

uint8_t ConsoleGetCursorPosition(void);
uint8_t ConsoleGetCursorLine(void);

void ConsoleSetDisplayWidth(uint16_t width);
void ConsoleSetDisplayHeight(uint16_t height);

uint16_t ConsoleGetDisplayWidth(void);
uint16_t ConsoleGetDisplayHeight(void);

uint8_t ConsoleWait(void);
void ConsoleClearKeyboardString(void);

void ConsolePrintKeyboardString(void);


// Parsing

uint8_t ConsoleSetPath(uint8_t* path);

uint8_t* ConsoleGetParameter(uint8_t index, uint8_t delimiter);


// Environment variables

void EnvironmentSetPathBin(uint8_t* path, uint8_t length);
uint8_t EnvironmentGetPathBinLength(void);
void EnvironmentGetPathBin(uint8_t* path, uint8_t length);
void EnvironmentSetHomeChar(uint8_t newHomeChar);
uint8_t EnvironmentGetHomeChar(void);

#endif

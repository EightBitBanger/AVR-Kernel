#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/command/cls/cls.h>

void functionCLS(uint8_t* param, uint8_t param_length) {
    
    ConsoleClearScreen(' ');
    
    ConsoleSetCursor(0, 0);
    
    return;
}

void registerCommandCLS(void) {
    
    uint8_t clsCommandName[] = "cls";
    
    ConsoleRegisterCommand(clsCommandName, sizeof(clsCommandName), functionCLS);
    
    return;
}

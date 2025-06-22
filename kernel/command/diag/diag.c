#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/command/diag/diag.h>

void functionDIAG(uint8_t* param, uint8_t param_length) {
    
    uint8_t msgTest[] = "Fuck";
    print(msgTest, sizeof(msgTest));
    
    return;
}

void registerCommandDIAG(void) {
    
    uint8_t diagCommandName[] = "diag";
    
    ConsoleRegisterCommand(diagCommandName, sizeof(diagCommandName), functionDIAG);
    
    return;
}

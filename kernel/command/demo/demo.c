#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/demo/demo.h>

void functionDEMO(uint8_t* param, uint8_t param_length) {
    
    uint8_t isActive = 1;
    
    while (isActive) {
        
        
        
        continue;
    }
    
    return;
}

void registerCommandDEMO(void) {
    
    uint8_t demoCommandName[] = "demo";
    
    ConsoleRegisterCommand(demoCommandName, sizeof(demoCommandName), functionDEMO);
    
    return;
}

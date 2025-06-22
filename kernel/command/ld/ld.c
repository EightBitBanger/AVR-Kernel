#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/ld/ld.h>

void functionLD(uint8_t* param, uint8_t param_length) {
    
    uint8_t msgDriverLoaded[]      = "Driver loaded";
    uint8_t msgDriverNotFound[]    = "File not found";
    uint8_t msgInvalidDriver[]     = "Invalid driver";
    uint8_t msgAlreadyLoaded[]     = "Already loaded";
    
    int8_t libState = LoadLibrary(param, param_length);
    
    if (libState == 0) {
        
        print(msgDriverNotFound, sizeof(msgDriverNotFound));
        printLn();
    }
    
    if (libState == 1) {
        
        print(msgDriverLoaded, sizeof(msgDriverLoaded));
        printLn();
    }
    
    if (libState == -1) {
        
        print(msgInvalidDriver, sizeof(msgInvalidDriver));
        printLn();
    }
    
    if (libState == -2) {
        
        print(msgAlreadyLoaded, sizeof(msgAlreadyLoaded));
        printLn();
    }
    
    return;
}



void registerCommandLD(void) {
    
    uint8_t ldCommandName[] = "ld";
    
    ConsoleRegisterCommand(ldCommandName, sizeof(ldCommandName), functionLD);
    
    return;
}

#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/command/mkdir/mkdir.h>

void functionMKDIR(uint8_t* param, uint8_t param_length) {
    
    uint8_t msgFileCreated[]  = "Directory created";
    uint8_t msgInvalidName[]  = "Invalid name";
    
    if ((param_length == 0) | 
        (param[0] == ' ')) {
        
        print(msgInvalidName, sizeof(msgInvalidName));
        printLn();
        
        return;
    }
    
    uint32_t directoryAddress = fsDirectoryCreate(param, param_length);
    
    fsDirectoryAddFile(fsWorkingDirectoryGetAddress(), directoryAddress);
    
    if (directoryAddress != 0) {
        
        print(msgFileCreated, sizeof(msgFileCreated));
        printLn();
        
    }
    
    return;
}

void registerCommandMKDIR(void) {
    
    uint8_t mkdirCommandName[] = "mkdir";
    
    ConsoleRegisterCommand(mkdirCommandName, sizeof(mkdirCommandName), functionMKDIR);
    
    return;
}

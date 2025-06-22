#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/command/mk/mk.h>

void functionMK(uint8_t* param, uint8_t param_length) {
    
    uint8_t msgFileCreated[]    = "File created";
    uint8_t msgErrorCreating[]  = "Error creating file";
    uint8_t msgInvalidName[]    = "Invalid filename";
    
    if ((param_length == 0) | 
        (param[0] == ' ')) {
        
        print(msgInvalidName, sizeof(msgInvalidName));
        printLn();
        
        return;
    }
    
    uint32_t fileAddress = fsFileCreate(param, param_length, 100);
    
    fsDirectoryAddFile(fsWorkingDirectoryGetAddress(), fileAddress);
    
    if (fileAddress == 0) {
        
        print(msgErrorCreating, sizeof(msgErrorCreating));
        printLn();
        
        return;
    }
    
    print(msgFileCreated, sizeof(msgFileCreated));
    printLn();
    
    return;
}

void registerCommandMK(void) {
    
    uint8_t mkCommandName[] = "mk";
    
    ConsoleRegisterCommand(mkCommandName, sizeof(mkCommandName), functionMK);
    
    return;
}

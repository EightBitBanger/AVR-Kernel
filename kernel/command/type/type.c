#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/type/type.h>

void functionType(uint8_t* param, uint8_t param_length) {
    
    uint32_t fileAddress = fsFileExists(param, param_length);
    
    if (fileAddress == 0) 
        return;
    
    int32_t index = fsFileOpen(fileAddress);
    
    uint32_t fileSize = fsFileGetSize(fileAddress);
    
    if (fileSize == 0) 
        return;
    
    uint8_t fileBuffer[fileSize];
    
    fsFileRead(index, fileBuffer, fileSize);
    
    fsFileClose(index);
    
    for (uint32_t i=0; i < fileSize; i++) {
        if (fileBuffer[i] == '\n') {
            printLn();
        } else {
            printChar( fileBuffer[i] );
        }
        
    }
    
    if (fileSize < 20) 
        printLn();
    
    return;
}



void registerCommandType(void) {
    
    uint8_t typeCommandName[] = "type";
    
    ConsoleRegisterCommand(typeCommandName, sizeof(typeCommandName), functionType);
    
    return;
}

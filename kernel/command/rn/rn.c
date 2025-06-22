#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/rn/rn.h>

uint8_t msgFileRenamed[]   = "File renamed";
uint8_t msgFileNotFound[]  = "File not found";

void functionRN(uint8_t* param, uint8_t param_length) {
    
    // Get source name
    uint8_t sourceName[10];
    uint8_t sourceNameLen = 0;
    
    for (uint8_t i=0; i < param_length; i++) {
        
        sourceNameLen++;
        
        sourceName[i] = param[i];
        
        if (param[i] == ' ') 
            break;
    }
    
    // Get new name
    uint8_t targetName[10];
    uint8_t targetNameLen = 0;
    
    for (uint8_t i=0; i < param_length; i++) {
        
        targetName[i] = param[i + sourceNameLen];
        
        targetNameLen++;
        
        if (i > sourceNameLen + 1) 
            break;
        
    }
    
    
    uint32_t fileAddress = fsFileExists(sourceName, sourceNameLen);
    
    if (fileAddress == 0) {
        
        print(msgFileNotFound, sizeof(msgFileNotFound));
        
        printLn();
        
        return;
    }
    
    fsFileSetName(fileAddress, targetName, targetNameLen);
    
    print(msgFileRenamed, sizeof(msgFileRenamed));
    
    printLn();
    
    return;
}

void registerCommandRN(void) {
    
    uint8_t rnCommandName[] = "rn";
    
    ConsoleRegisterCommand(rnCommandName, sizeof(rnCommandName), functionRN);
    
    return;
}

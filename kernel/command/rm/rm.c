#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/rm/rm.h>

void functionRM(uint8_t* param, uint8_t param_length) {
    
    uint8_t msgFileNotFound[] = "File not found";
    uint8_t msgAccessDenied[] = "Access denied";
    uint8_t msgFileRemoved[] = "File removed";
    uint8_t msgDirRemoved[] = "Directory removed";
    uint8_t msgInvalidName[] = "Invalid filename";
    
    if ((param_length == 0) | 
        (param[0] == ' ')) {
        
        print(msgInvalidName, sizeof(msgInvalidName));
        printLn();
        
        return;
    }
    
    struct FSAttribute attrib;
    
    uint32_t fileAddress = fsFileExists(param, param_length);
    uint32_t directoryAddress = 0;
    
    if (fileAddress == 0) {
        
        directoryAddress = fsDirectoryExists(param, param_length);
        
        if (directoryAddress == 0) {
            
            print(msgFileNotFound, sizeof(msgFileNotFound));
            printLn();
            
            return;
        } else {
            
            fsFileGetAttributes(directoryAddress, &attrib);
            
        }
        
    } else {
        
        fsFileGetAttributes(fileAddress, &attrib);
        
    }
    
    // Check read only attribute
    if (attrib.writeable != 'w') {
        
        print(msgAccessDenied, sizeof(msgAccessDenied));
        printLn();
        
        return;
    }
    
    if (fileAddress != 0) {
        
        uint32_t fileRefAddress = fsFileDelete(param, param_length);
        
        if (fileRefAddress != 0) {
            
            if (fsDirectoryRemoveFile( fsWorkingDirectoryGetAddress(), fileRefAddress ) == 1) {
                
                print(msgFileRemoved, sizeof(msgFileRemoved));
                printLn();
                
            }
            
            return;
        }
        
    }
    
    if (directoryAddress != 0) {
        
        uint32_t fileRefAddress = fsDirectoryDelete(param, param_length);
        
        if (fileRefAddress != 0) {
            
            fsDirectoryRemoveFile( fsWorkingDirectoryGetAddress(), fileRefAddress );
            
            print(msgDirRemoved, sizeof(msgDirRemoved));
            printLn();
            
            return;
        }
        
    }
    
    print(msgFileNotFound, sizeof(msgFileNotFound));
    printLn();
    
    return;
}

void registerCommandRM(void) {
    
    uint8_t rmCommandName[] = "rm";
    
    ConsoleRegisterCommand(rmCommandName, sizeof(rmCommandName), functionRM);
    
    return;
}

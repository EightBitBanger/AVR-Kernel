#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/cd/cd.h>

void functionCD(uint8_t* param, uint8_t param_length) {
    uint8_t deviceLetter = param[0];
    uppercase(&deviceLetter);
    
    // Get current device
    struct Partition part = fsDeviceOpen( fsDeviceGetCurrent() );
    DirectoryHandle currentDirectory = fsWorkingDirectoryGetCurrent();
    
    //
    // Drop down the parent directory
    //
    
    if ((param[0] == '.') & (param[1] == '.') & (param[2] == ' ')) {
        if (fsWorkingDirectoryGetIndex() == 0) 
            return;
        
        fsWorkingDirectorySetParent();
        
        uint8_t dirname[] = "          ";
        DirectoryHandle currentDirectory = fsWorkingDirectoryGetCurrent();
        fsFileGetName(part, currentDirectory, dirname);
        
        ConsoleSetPath(dirname);
        
        return;
    }
    
    // Drop to the root directory
    if ((param[0] == '/') & (param[1] == ' ')) {
        uint8_t consolePrompt[] = "x>";
        ConsoleSetPrompt(consolePrompt, sizeof(consolePrompt));
        
        fsWorkingDirectorySetRoot( fsDeviceGetRootDirectory(part) );
        return;
    }
    
    // Change the directory
    
    if (fsFindDirectory(part, currentDirectory, param) == 0) {
        uint8_t msgDirectoryNotFound[]  = "Directory not found";
        print(msgDirectoryNotFound, sizeof(msgDirectoryNotFound));
        printLn();
        return;
    }
    
    ConsoleSetPath(param);
    return;
}

void registerCommandCD(void) {
    
    uint8_t cdCommandName[] = "cd";
    
    ConsoleRegisterCommand(cdCommandName, sizeof(cdCommandName), functionCD);
    
    return;
}

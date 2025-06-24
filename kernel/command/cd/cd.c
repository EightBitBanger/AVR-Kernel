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
        
        DirectoryHandle targetDirectory = fsWorkingDirectoryGetCurrent();
        
        
        uint8_t filename[20];
        for (uint8_t i=0; i < 20; i++) 
            filename[i] = ' ';
        filename[0] = 'x';
        filename[1] = '/';
        fsFileGetName(part, targetDirectory, &filename[2]);
        
        uint8_t namelenth = 0;
        for (uint8_t i=0; i < 20; i++) {
            if (filename[i] != ' ') 
                continue;
            filename[i] = '>';
            namelenth = i + 2; // For the end and the last char
            break;
        }
        
        ConsoleSetPrompt(filename, namelenth);
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
    
    uint32_t targetDirectory = fsFindDirectory(part, currentDirectory, param);
    if (targetDirectory == 0) {
        uint8_t msgDirectoryNotFound[]  = "Directory not found";
        print(msgDirectoryNotFound, sizeof(msgDirectoryNotFound));
        printLn();
        return;
    }
    
    fsWorkingDirectoryChange(targetDirectory);
    
    uint8_t filename[20];
    for (uint8_t i=0; i < 20; i++) 
        filename[i] = ' ';
    filename[0] = 'x';
    filename[1] = '/';
    fsFileGetName(part, targetDirectory, &filename[2]);
    
    uint8_t namelenth = 0;
    for (uint8_t i=0; i < 20; i++) {
        if (filename[i] != ' ') 
            continue;
        filename[i] = '>';
        namelenth = i + 2; // For the end and the last char
        break;
    }
    
    ConsoleSetPrompt(filename, namelenth);
    return;
}

void registerCommandCD(void) {
    
    uint8_t cdCommandName[] = "cd";
    
    ConsoleRegisterCommand(cdCommandName, sizeof(cdCommandName), functionCD);
    
    return;
}

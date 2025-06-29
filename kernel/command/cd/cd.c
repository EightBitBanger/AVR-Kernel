#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/cd/cd.h>
void SetPromptName(struct Partition part, uint8_t isRoot, DirectoryHandle targetDirectory) {
    uint8_t filename[20];
    for (uint8_t i=0; i < 20; i++) 
        filename[i] = ' ';
    filename[0] = 'x';
    
    if (isRoot == 0) {
        filename[1] = '/';
        fsFileGetName(part, targetDirectory, &filename[2]);
    }
    
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

void functionCD(uint8_t* param, uint8_t param_length) {
    uint8_t deviceLetter = param[0];
    uppercase(&deviceLetter);
    
    // Get current device
    struct Partition part = fsDeviceOpen( fsDeviceGetCurrent() );
    DirectoryHandle currentDirectory = fsWorkingDirectoryGetCurrent();
    
    // Drop down the parent directory
    if ((param[0] == '.') & (param[1] == '.') & (param[2] == ' ')) {
        uint8_t isRoot = 0;
        if (fsWorkingDirectorySetParent() == 0) 
            isRoot = 1;
        DirectoryHandle targetDirectory = fsWorkingDirectoryGetCurrent();
        SetPromptName(part, isRoot, targetDirectory);
        return;
    }
    
    // Drop to the root directory
    if ((param[0] == '/') & (param[1] == ' ')) {
        uint8_t consolePrompt[] = "x>";
        ConsoleSetPrompt(consolePrompt, sizeof(consolePrompt));
        
        fsWorkingDirectorySetRoot( part, fsDeviceGetRootDirectory(part) );
        return;
    }
    
    // Check if the directory exists
    uint32_t targetDirectory = fsFindDirectory(part, currentDirectory, param);
    if (targetDirectory == 0) {
        uint8_t msgDirectoryNotFound[]  = "Directory not found";
        print(msgDirectoryNotFound, sizeof(msgDirectoryNotFound));
        printLn();
        return;
    }
    
    // Change the directory
    fsWorkingDirectoryChange(part, targetDirectory);
    
    SetPromptName(part, 0, targetDirectory);
    return;
}

void registerCommandCD(void) {
    
    uint8_t cdCommandName[] = "cd";
    
    ConsoleRegisterCommand(cdCommandName, sizeof(cdCommandName), functionCD);
    
    return;
}

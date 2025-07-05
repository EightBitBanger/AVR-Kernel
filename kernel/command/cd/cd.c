#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/cd/cd.h>
void SetPromptPath(struct Partition part, uint8_t isRoot, DirectoryHandle targetDirectory);

void functionCD(uint8_t* param, uint8_t param_length) {
    uint8_t deviceLetter = param[0];
    uppercase(&deviceLetter);
    
    // Get current device
    struct Partition part = fsDeviceOpen( fsDeviceGetCurrent() );
    DirectoryHandle currentDirectory = fsWorkingDirectoryGetCurrent();
    
    // Drop down to the parent directory
    if ((param[0] == '.') & (param[1] == '.') & (param[2] == ' ')) {
        uint8_t isRoot = 0;
        if (fsWorkingDirectorySetParent() == 0) 
            isRoot = 1;
        DirectoryHandle targetDirectory = fsWorkingDirectoryGetCurrent();
        SetPromptPath(part, isRoot, targetDirectory);
        return;
    }
    
    // Drop to the root directory
    if ((param[0] == '/' && param[1] == ' ')) {
        DirectoryHandle rootDirectory = fsDeviceGetRootDirectory(part);
        fsWorkingDirectorySetRoot( part, rootDirectory );
        SetPromptPath(part, 1, rootDirectory);
        return;
    }
    
    // Change the device
    if ((param[1] == ':' && param[2] == ' ')) {
        lowercase(&param[0]);
        if (!is_letter(&param[0])) 
            return;
        uint8_t consolePrompt[] = "x>";
        
        // Check root memory device
        if (param[0] == 'x') {
            fsDeviceSetBase(0);
            ConsoleSetPrompt(consolePrompt, sizeof(consolePrompt));
            return;
        }
        SetPromptPath(part, 1, nullptr);
        // Set to device offset
        fsDeviceSetBase(PERIPHERAL_ADDRESS_BEGIN + (PERIPHERAL_STRIDE * (param[0] - 'a')));
        consolePrompt[0] = param[0];
        ConsoleSetPrompt(consolePrompt, sizeof(consolePrompt));
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
    
    SetPromptPath(part, 0, targetDirectory);
    return;
}

void registerCommandCD(void) {
    
    uint8_t cdCommandName[] = "cd";
    
    ConsoleRegisterCommand(cdCommandName, sizeof(cdCommandName), functionCD);
    
    return;
}


void SetPromptPath(struct Partition part, uint8_t isRoot, DirectoryHandle targetDirectory) {
    uint8_t filename[20];
    for (uint8_t i=0; i < 20; i++) 
        filename[i] = ' ';
    uint32_t baseAddress = fsDeviceGetBase();
    if (baseAddress < PERIPHERAL_ADDRESS_BEGIN) {
        filename[0] = 'x';
    } else {
        filename[0] = ((baseAddress - PERIPHERAL_ADDRESS_BEGIN) / 0x10000) + 'a';
    }
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

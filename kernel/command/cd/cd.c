#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/cd/cd.h>

void functionCD(uint8_t* param, uint8_t param_length) {
    uint8_t deviceLetter = param[0];
    uppercase(&deviceLetter);
    
    // Get current device
    fsDeviceGetCurrent();
    struct Partition part = fsDeviceOpen(0x00000000);
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
    
    // The letter represents the hardware address 
    // offset pointing to the device on the system bus
    
    if ((deviceLetter >= 'A') & (deviceLetter <= 'Z') & (param[1] == ':')) {
        uint32_t device_address = fsDeviceSetCurrent(deviceLetter);
        struct Partition checkPart = fsDeviceOpen(device_address);
        if (checkPart.block_size == 0) {
            uint8_t msgDeviceNotFound[]  = "Device not found";
            print(msgDeviceNotFound, sizeof(msgDeviceNotFound));
            printLn();
            return;
        }
        
        return;
    }
    
    // Drop to the root directory
    if ((param[0] == '/') & (param[1] == ' ')) {
        //uint32_t device_address = fsDeviceGetCurrent();
        
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
    
    /*
    
    if (param_length > FILE_NAME_LENGTH) 
        param_length = FILE_NAME_LENGTH;
    
    uint8_t msgDirectoryNotFound[]  = "Directory not found";
    
    uint8_t deviceLetter = param[0];
    uppercase(&deviceLetter);
    
    
    //
    // Drop down the parent directory
    //
    
    if ((param[0] == '.') & (param[1] == '.') & (param[2] == ' ')) {
        
        if (fsWorkingDirectoryGetStack() > 1) {
            
            uint8_t promptLength = 0;
            uint8_t PromptDir[20] = {fsDeviceGetRootLetter(), '/'};
            
            if (fsWorkingDirectorySetToParent() == 1) {
                
                uint8_t directoryName[FILE_NAME_LENGTH];
                
                for (uint8_t i=0; i < FILE_NAME_LENGTH; i++) 
                    directoryName[i] = ' ';
                
                fsFileGetName(fsWorkingDirectoryGetAddress(), directoryName);
                
                for (uint8_t n=0; n < FILE_NAME_LENGTH; n++) {
                    
                    PromptDir[n + 2] = directoryName[n];
                    
                    if (PromptDir[n + 2] == ' ') {
                        
                        promptLength = n + 1;
                        break;
                    }
                    
                }
                
                PromptDir[promptLength + 1] = '>';
                
                ConsoleSetPrompt(PromptDir, promptLength + 3);
            }
            
        } else {
            
            uint8_t PromptRoot[] = {fsDeviceGetRootLetter(), '>'};
            ConsoleSetPrompt(PromptRoot, sizeof(PromptRoot)+1);
            
            fsWorkingDirectoryClear();
            
        }
        
        return;
    }
    
    
    
    //
    // Change Device
    //
    
    // The letter represents the hardware address 
    // offset pointing to the device on the system bus
    
    if ((deviceLetter >= 'A') & (deviceLetter <= 'Z') & (param[1] == ':')) {
        
        fsDeviceSetRootLetter( deviceLetter );
        
        uint8_t consolePrompt[] = {deviceLetter, '>'};
        
        ConsoleSetPrompt(consolePrompt, sizeof(consolePrompt)+1);
        
        fsWorkingDirectoryClear();
        
        return;
    }
    
    
    //
    // Change Directory
    //
    
    if ((deviceLetter >= 'A') & (deviceLetter <= 'Z')) {
        
        param[param_length+1] = '\0';
        
        if (vfsLookup(param, param_length) == 0) {
            print(msgDirectoryNotFound, sizeof(msgDirectoryNotFound));
            printLn();
            return;
        }
        
        // Change prompt path name
        ConsoleSetPath(param);
        
        return;
    }
    
    
    //
    // Drop down to the root
    //
    
    if ((param[0] == '/') & (param[1] == ' ')) {
        
        uint8_t PromptRoot[] = {fsDeviceGetRootLetter(), '>'};
        ConsoleSetPrompt(PromptRoot, sizeof(PromptRoot)+1);
        
        fsWorkingDirectoryClear();
        
        return;
    }
    
    
    //
    // Display the full path
    //
    
    uint32_t currentDirectory = fsWorkingDirectoryGetAddress();
    
    uint8_t numberOfDirectories = fsWorkingDirectoryGetStack();
    
    uint32_t parentChain[numberOfDirectories];
    
    for (uint8_t i=0; i < numberOfDirectories; i++) {
        
        parentChain[i] = currentDirectory;
        
        currentDirectory = fsDirectoryGetParent(currentDirectory);
        
    }
    
    // Print the directories in reverse
    printChar('/');
    for (uint8_t i=0; i < numberOfDirectories; i++) {
        
        uint8_t directoryName[FILE_NAME_LENGTH];
        for (uint8_t a=0; a < FILE_NAME_LENGTH; a++) 
            directoryName[a] = ' ';
        
        fsFileGetName(parentChain[(numberOfDirectories - 1) - i], directoryName);
        
        for (uint8_t c=0; c < FILE_NAME_LENGTH; c++) {
            
            if (directoryName[c] == ' ') 
                break;
            
            printChar( directoryName[c] );
            
        }
        
        if (i < numberOfDirectories - 1) 
            printChar('/');
    }
    
    printLn();
    */
    
    return;
}

void registerCommandCD(void) {
    
    uint8_t cdCommandName[] = "cd";
    
    ConsoleRegisterCommand(cdCommandName, sizeof(cdCommandName), functionCD);
    
    return;
}

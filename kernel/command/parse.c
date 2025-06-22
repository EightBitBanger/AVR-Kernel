#include <avr/io.h>

#include <kernel/delay.h>

#include <kernel/command/cli.h>

extern uint8_t console_string[CONSOLE_STRING_LENGTH];
extern uint8_t parameters_begin;

/// Set the console prompt to the last name in the path
/// This helps preserve space on a small display.
uint8_t ConsoleSetPath(uint8_t* path) {
    
    struct Partition part = fsDeviceOpen(0x00000000);
    uint32_t currentDirectory = fsWorkingDirectoryGetCurrent();
    
    uint32_t subDirectoryAddress = fsFindDirectory(part, currentDirectory, path);
    
    // Clear the string
    uint8_t directoryName[FILE_NAME_LENGTH];
    for (uint8_t i=0; i < FILE_NAME_LENGTH; i++) 
        directoryName[i] = ' ';
    
    uint8_t promptLength = 0;
    uint8_t PromptDir[20] = {'x', ' '};
    
    if (subDirectoryAddress != 0) {
        fsFileGetName(part, subDirectoryAddress, directoryName);
        fsWorkingDirectoryChange(subDirectoryAddress);
        
        PromptDir[1] = '/';
        
        // Get the directory name for prompt string
        for (uint8_t n=0; n < FILE_NAME_LENGTH; n++) {
            PromptDir[n + 2] = directoryName[n];
            
            if (PromptDir[n + 2] == ' ') {
                promptLength = n + 1;
                break;
            }
            
        }
    }
    
    
    PromptDir[promptLength + 1] = '>';
    
    ConsoleSetPrompt(PromptDir, promptLength + 3);
    
    /*
    uint8_t promptLength = 0;
    uint8_t PromptDir[20] = {fsDeviceGetRootLetter(), '/'};
    
    // Clear the string
    uint8_t directoryName[FILE_NAME_LENGTH];
    for (uint8_t i=0; i < FILE_NAME_LENGTH; i++) 
        directoryName[i] = ' ';
    
    // Find the last occurrence of '/' in the path
    uint8_t* last_slash = (uint8_t*)strrchr((const char*)path, '/');
    
    // Check only one '/', use full path
    if (last_slash == NULL) {
        
        fsFileGetName(fsWorkingDirectoryGetAddress(), directoryName);
        
    } else {
        
        // Get the substring after the last '/'
        uint8_t* last_dir = last_slash + 1;
        
        // Copy the last component into a stack-allocated array
        char result[1024];  // Make sure it's large enough to hold the last component
        
        // Ensure that the last component fits in the buffer (this handles overflows)
        strncpy((char*)result, (const char*)last_dir, sizeof(result) - 1);  // Prevent buffer overflow
        result[sizeof(result) - 1] = '\0';  // Null-terminate the string
        
        for (uint8_t i=0; i < FILE_NAME_LENGTH; i++) {
            if (result[i] == '\0') 
                break;
            
            directoryName[i] = result[i];
        }
        
    }
    
    // Set the prompt string
    
    for (uint8_t n=0; n < FILE_NAME_LENGTH; n++) {
        PromptDir[n + 2] = directoryName[n];
        
        if (PromptDir[n + 2] == ' ') {
            promptLength = n + 1;
            break;
        }
        
    }
    
    PromptDir[promptLength + 1] = '>';
    
    ConsoleSetPrompt(PromptDir, promptLength + 3);
    */
    return 1;
}


uint8_t* ConsoleGetParameter(uint8_t index, uint8_t delimiter) {
    int16_t numrerOfParams = -1;
    
    // Loop through the string until the first blank character
    for (uint8_t i=0; i < CONSOLE_STRING_LENGTH - parameters_begin; i++) {
        if (console_string[parameters_begin + i] == delimiter) 
            numrerOfParams++;
        
        // Find the parameters index
        if (numrerOfParams == index) {
            
            return &console_string[parameters_begin + i] + 1;
        }
        
    }
    
    return 0;
}


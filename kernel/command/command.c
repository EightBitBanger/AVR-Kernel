#include <avr/io.h>
#include <kernel/command/cli.h>

extern uint32_t fs_working_directory_address;

extern uint16_t display_width;
extern uint16_t display_height;

extern uint8_t console_string[CONSOLE_STRING_LENGTH];
extern uint8_t console_string_old[CONSOLE_STRING_LENGTH];
extern uint8_t console_string_length;
extern uint8_t console_string_length_old;

extern uint8_t parameters_begin;

extern uint8_t console_position;
extern uint8_t console_line;

extern uint8_t oldScanCodeLow;
extern uint8_t oldScanCodeHigh;

extern uint8_t lastChar;

extern uint8_t shiftState;

extern uint8_t console_prompt[16];
extern uint8_t console_prompt_length;

extern uint8_t cursor_blink_rate;

extern uint16_t displayRows;
extern uint16_t displayColumbs;

extern struct Driver* displayDevice;
extern struct Driver* keyboadDevice;

extern struct ConsoleCommand CommandRegistry[CONSOLE_FUNCTION_TABLE_SIZE];

uint8_t msgDeviceNotReady[] = "Device not ready";
uint8_t msgBadCommand[] = "Bad cmd or filename";

uint8_t CommandFunctionLookup(uint8_t params_begin);
uint8_t ExecuteFile(struct Partition part, FileHandle fileAddress);
uint8_t ExecuteBinDirectory(void);

uint8_t currentDeviceLetter;
uint32_t currentWorkingDirectoryAddress;
uint32_t currentWorkingDirectoryStack;


void KeyFunctionReturn(void) {
    printLn();
    ConsoleSetCursorPosition(0);
    
    // Save last entered command string
    for (uint8_t a=0; a <= console_string_length; a++) 
        console_string_old[a] = console_string[a];
    
    console_string_length_old = console_string_length;
    
    // Get the beginning of the function parameters
    parameters_begin = 0;
    uint8_t parameters_length = 0;
    for (uint8_t i=0; i < console_string_length; i++) {
        if (console_string[i] != ' ') 
            continue;
        parameters_begin = i + 1;
        parameters_length = console_string_length - i;
        break;
    }
    
    // Separate the function name
    uint8_t functionName[FILE_NAME_LENGTH];
    for (uint8_t i=0; i <= console_string_length; i++) 
        functionName[i] = ' ';
    for (uint8_t i=0; i <= console_string_length; i++) {
        if (console_string[i] == ' ' || console_string[i] == '\0') {
            functionName[i] = '\0';
            break;
        }
        functionName[i] = console_string[i];
    }
    
    uint8_t passed = 0;
    
    // Change device with a colon character
    if (console_string[1] == ':') {
        lowercase(&console_string[0]);
        
        // Device letter
        if (console_string[0] >= 'a' && console_string[0] <= 'z') {
            
            // Update the prompt if the device letter was changed
            uint8_t consolePrompt[] = {console_string[0], '>', '\0'};
            ConsoleSetPrompt(consolePrompt, sizeof(consolePrompt));
            
            // Check memory storage
            if (console_string[0] == 'x') {
                fsDeviceSetBase(0x00000);
                struct Partition part = fsDeviceOpen(0x00000);
                fsWorkingDirectorySetRoot(part, fsDeviceGetRootDirectory(part));
            } else {
                // Set the new device base address
                fsDeviceSetBase(PERIPHERAL_ADDRESS_BEGIN + (PERIPHERAL_STRIDE * (console_string[0] - 'a')));
                struct Partition part = fsDeviceOpen(0x00000);
                fsWorkingDirectorySetRoot(part, fsDeviceGetRootDirectory(part));
            }
            ConsoleSetCursorPosition(2);
            console_string_length = 0;
            passed = 1;
        }
    }
    
    // Look up function name
    if (passed == 0) 
        passed = CommandFunctionLookup(parameters_begin);
    
    // Check device is ready
    struct Partition part = fsDeviceOpen(0x00000);
    if (passed == 0 && part.block_size == 0) {
        ConsoleSetCursorPosition(0);
        print(msgDeviceNotReady, sizeof(msgDeviceNotReady));
        printLn();
        passed = 1;
    }
    
    // Check file executable
    if (passed == 0 && console_string_length_old > 0) {
        DirectoryHandle currentDirectory = fsWorkingDirectoryGetCurrent();
        FileHandle fileHandle = fsDirectoryFindByName(part, currentDirectory, functionName);
        if (fileHandle != 0) {
            if (ExecuteFile(part, fileHandle) != 0) 
                passed = 1;
        }
    }
    
    // Check bin directory
    if (passed == 0 && console_string_length_old > 0) {
        uint32_t baseAddress = fsDeviceGetBase();
        
        uint32_t homeDevice = fsEnvironmentGetHomeDevice();
        if (homeDevice != 0) 
        fsDeviceSetBase(homeDevice);
        
        struct Partition homePart = fsDeviceOpen(0x00000);
        DirectoryHandle homeRootDirectory = fsDeviceGetRootDirectory(homePart);
        
        uint32_t programSize=0;
        
        // Find the bin directory
        DirectoryHandle binDirectory = fsDirectoryFindByName(homePart, homeRootDirectory, (uint8_t*)"bin");
        if (binDirectory != 0) {
            // Find the file
            FileHandle fileHandle = fsDirectoryFindByName(homePart, binDirectory, functionName);
            if (fileHandle != 0) {
                File index = fsFileOpen(homePart, fileHandle);
                programSize = fsFileGetSize(homePart, fileHandle);
                uint8_t programBuffer[programSize];
                
                fsFileRead(homePart, index, programBuffer, programSize);
                fsFileClose(index);
                
                // Run the program
                fsDeviceSetBase(baseAddress);
                EmulatorSetParameters(&console_string[parameters_begin], parameters_length);
                EmulatorSetProgram(programBuffer, programSize);
                
                EmulateX4( EVENT_NOMESSAGE );
                passed = 1;
            }
        }
        
        // Return to the original device and directory
        fsDeviceSetBase(baseAddress);
    }
    
    // Bad command or filename
    if (passed == 0 && console_string_length_old > 0) {
        print( msgBadCommand, sizeof(msgBadCommand) );
        printLn();
    }
    
    printPrompt();
    ConsoleClearKeyboardString();
    
    console_string_length = 0;
}


uint8_t CommandFunctionLookup(uint8_t params_begin) {
    uint8_t passed = 0;
    uint8_t parameters_begin_chk = 0;
    
    for (uint8_t i=0; i < CONSOLE_FUNCTION_TABLE_SIZE; i++) {
        
        for (uint8_t n=0; n < CONSOLE_FUNCTION_NAME_LENGTH; n++) {
            uint8_t consoleChar = console_string[n];
            lowercase(&consoleChar);
            
            if (CommandRegistry[i].name[n] != consoleChar) {
                passed = 0;
                break;
            }
            if ((is_symbol(&console_string[n+1]) == 1) | 
                (CommandRegistry[i].name[n] == ' ')) {
                
                if (parameters_begin_chk == 0) 
                    parameters_begin_chk = n + 1;
                break;
            }
            
            passed = 1;
        }
        if (passed == 0) 
            continue;
        if (parameters_begin_chk != 0) 
            params_begin = parameters_begin_chk;
        
        // Run the function
        if (CommandRegistry[i].function != nullptr) 
            CommandRegistry[i].function( &console_string[params_begin], (console_string_length + 1) - params_begin );
        
        break;
    }
    
    return passed;
}


uint8_t ExecuteFile(struct Partition part, FileHandle fileAddress) {
    uint8_t attributes[4];
    fsFileGetAttributes(part, fileAddress, attributes);
    if (attributes[3] == 'd' || attributes[0] != 'x' || attributes[1] != 'r') 
        return 0;
    
    uint32_t programSize = fsFileGetSize(part, fileAddress);
    if (programSize == 0) 
        return 0;
    
    File index = fsFileOpen(part, fileAddress);
    uint8_t programBuffer[programSize];
    fsFileRead(part, index, programBuffer, programSize);
    
    // Launch the emulator
    EmulatorSetProgram( programBuffer, programSize );
    EmulateX4( EVENT_NOMESSAGE );
    
    fsFileClose(index);
    return 1;
}


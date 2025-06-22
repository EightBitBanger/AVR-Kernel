#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <kernel/delay.h>
#include <kernel/kernel.h>

#include <kernel/emulation/x4/x4.h>


// Program memory segment
extern uint8_t programBuffer[MAX_PROGRAM_SIZE];
extern uint32_t programSize;

// Parameters from the console
extern uint8_t param_string[32];
extern uint32_t param_length;

extern uint8_t reg[8];        // Registers
extern uint8_t stack[100];    // Temporary stack implementation

// Counters
extern uint32_t pc;           // Program counter
extern uint32_t pcRet;        // Where to return from a call
extern uint32_t stack_ptr;
// Flags
extern uint8_t flag_compare;
extern uint8_t flag_greater;
extern uint8_t flag_consoleDirty;


void ISC_OperatingSystem(void) {
    
    // Dynamically load a driver onto the driver table
    if (reg[rAH] == 0x0A) {
        
        int8_t status = LoadLibrary(param_string, param_length);
        
        if (status == -1) {reg[rBL] = 1; return;}   // File does not exist
        if (status == -2) {reg[rBL] = 2; return;}   // Corrupt driver
        if (status == -3) {reg[rBL] = 3; return;}   // Already loaded
        
        if (status >= 0) 
            reg[rBL] = 0;
        return;
    }
    
    return;
}


void ISC_DisplayRoutine(void) {
    
    // AH = 0x09 Print string
    if (reg[rAH] == 0x09) {
        union Pointer dataOffset;
        dataOffset.byte_t[3] = 0;
        dataOffset.byte_t[2] = 0;
        dataOffset.byte_t[1] = reg[rDH];
        dataOffset.byte_t[0] = reg[rDL];
        
        for (uint32_t i=0; i < programSize; i++) {
            if (programBuffer[dataOffset.address + i] == '\0') 
                break;
            if (programBuffer[dataOffset.address + i] == '\n') {
                printLn();
                continue;
            }
            printChar(programBuffer[dataOffset.address + i]);
        }
        
        flag_consoleDirty = 1;
        return;
    }
    
    // AH = 0x03 Clear display
    if (reg[rAH] == 0x03) {
        
        ConsoleClearScreen(' ');
        ConsoleSetCursor(0, 0);
        
        flag_consoleDirty = 0;
        return;
    }
    reg[rBL] = 1;
    return;
}


void ISC_FileSystemRoutine(void) {
    /*
    if (param_string[0] == ' ' || 
        param_length == 0) {
        reg[rBL] = 1;
        return;
    }
    
    // AH = 0x3C Create a file
    if (reg[rAH] == 0x3C) {
        
        // Get file size from the CX register
        union Pointer dataOffset;
        dataOffset.byte_t[3] = 0;
        dataOffset.byte_t[2] = 0;
        dataOffset.byte_t[1] = reg[rCH];
        dataOffset.byte_t[0] = reg[rCL];
        
        // Check file exists
        uint32_t fileAddress = fsFileExists(param_string, param_length);
        if (fileAddress != 0) {
            reg[rBL] = 2;
            return;
        }
        
        // Create the file
        fileAddress = fsFileCreate(param_string, param_length, dataOffset.address);
        if (fileAddress == 0) {
            reg[rBL] = 3;
            return;
        }
        
        reg[rBL] = 0;
        flag_consoleDirty = 0;
        return;
    }
    
    // AH = 0x39 Create a directory
    if (reg[rAH] == 0x39) {
        
        // Check directory exists
        uint32_t directoryAddress = fsDirectoryExists(param_string, param_length);
        if (directoryAddress != 0) {
            reg[rBL] = 2;
            return;
        }
        
        // Create the directory
        directoryAddress = fsDirectoryCreate(param_string, param_length);
        if (directoryAddress == 0) {
            reg[rBL] = 3;
            return;
        }
        
        reg[rBL] = 0;
        flag_consoleDirty = 0;
        return;
    }
    
    // AH = 0x41 Delete a file
    if (reg[rAH] == 0x41) {
        
        uint32_t fileAddress = fsFileDelete(param_string, param_length);
        if (fileAddress == 0) {
            reg[rBL] = 2;
            return;
        }
        
        reg[rBL] = 0;
        flag_consoleDirty = 0;
        return;
    }
    
    // AH = 0x3A Delete a directory
    if (reg[rAH] == 0x3A) {
        
        uint32_t directoryAddress = fsDirectoryDelete(param_string, param_length);
        if (directoryAddress == 0) {
            reg[rBL] = 2;
            return;
        }
        
        reg[rBL] = 0;
        flag_consoleDirty = 0;
        return;
    }
    
    // AH = 0x4C Print a file
    if (reg[rAH] == 0x4C) {
        uint32_t fileAddress = fsFileExists(param_string, param_length);
        if (fileAddress != 0) {
            int32_t file = fsFileOpen(fileAddress);
            uint32_t fileSz = fsFileGetSize(fileAddress);
            
            uint8_t fileBuffer[fileSz];
            fsFileRead(file, fileBuffer, fileSz);
            for (unsigned int i=0; i < fileSz; i++) {
                if (ConsoleGetCursorPosition() >= ConsoleGetDisplayWidth()) 
                    printLn();
                
                if (fileBuffer[i] == '\n') {
                    printLn();
                    continue;
                }
                printChar(fileBuffer[i]);
            }
            
            fsFileClose(file);
            reg[rBL] = 0;
            return;
        }
        reg[rBL] = 2;
        return;
    }
    */
    /*
    // AH = 0x3B Change the current working directory
    if (reg[rAH] == 0x3B) {
        
        //vfsLookup(param_string, param_length);
        //ConsoleSetPath(param_string);
        
        // Update the global current working directory
        //extern uint32_t currentWorkingDirectoryAddress;
        //extern uint32_t currentWorkingDirectoryAddress;
        //extern uint32_t currentWorkingDirectoryStack;
        
        //currentWorkingDirectoryAddress = fsWorkingDirectoryGetAddress();
        //currentWorkingDirectoryStack   = fsWorkingDirectoryGetStack();
        
        flag_consoleDirty = 0;
        
        reg[rBL] = 0;
        return;
    }
    
    // AH = 0x3C Show current directory path
    if (reg[rAH] == 0x3F) {
        flag_consoleDirty = 1;
        reg[rBL] = 0;
        return;
    }
    */
    
    reg[rBL] = 1;
    return;
}


void ISC_NetworkRoutine(void) {
    
    // AH = 0x0A Initiate the network device
    if (reg[rAH] == 0x0A) {
        if (ntInit() == 0) {
            reg[rBL] = 1;
            return;
        }
        if (ntCheckInitiated() == 0) {
            reg[rBL] = 2;
            return;
        }
        
        reg[rBL] = 0;
        return;
    }
    
    // AH = 0x0C Send a message
    // AL = 0 Data from the file
    // AL = 1 Data from the parameter string
    if (reg[rAH] == 0x0C) {
        union Pointer offset;
        offset.byte_t[3] = 0;
        offset.byte_t[2] = 0;
        offset.byte_t[1] = reg[rDH];
        offset.byte_t[0] = reg[rDL];
        
        // Check message bounds
        if (offset.address >= programSize || offset.address + reg[rCL] >= programSize) {
            reg[rBL] = 1;
            return;
        }
        
        // Select data source
        switch (reg[rAL]) {
            case 0x00: ntSend(&programBuffer[offset.address], reg[rCL]); break;
            case 0x01: ntSend(param_string, param_length); break;
        }
        
        reg[rBL] = 0;
        return;
    }
    
    reg[rBL] = 1;
    return;
}


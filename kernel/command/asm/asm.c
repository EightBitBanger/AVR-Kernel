#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/list/list.h>

void functionAsm(uint8_t* param, uint8_t param_length) {
    
    /*
    
    uint8_t msgPromptString[] = "-";
    
    uint8_t asm_console_string[40];
    
    uint8_t asm_console_string_length;
    
    uint8_t assemblyState = 0;
    uint32_t assemblyAddress = 0;
    uint32_t disassemblyAddress = 0;
    uint32_t dumpAddress = 0;
    
    uint8_t lastChar         = ConsoleGetRawChar();
    uint8_t console_position = ConsoleGetCursorPosition();
    uint8_t console_line     = ConsoleGetCursorLine();
    
    for (uint8_t i=0; i < 40; i++) 
        asm_console_string[i] = ' ';
    
    asm_console_string_length = 0;
    
    // Open the file
    uint32_t fileAddress = fsFileExists(param, param_length);
    
    if (fileAddress == 0) {
        
        if (param[0] == ' ') 
            return;
        
        fileAddress = fsFileCreate(param, param_length, 1);
        
        fsDirectoryAddFile( fsWorkingDirectoryGetAddress(), fileAddress );
        
        if (fileAddress == 0) 
            return;
        
    }
    
    fsFileOpen( fileAddress );
    
    uint32_t fileSize = fsFileGetSize( fileAddress );
    
    // File data buffer
    uint8_t fileBuffer[1024];
    
    for (uint16_t i=0; i < fileSize; i++) 
        fileBuffer[i] = ' ';
    
    // Load the file into memory
    fsFileRead(fileBuffer, fileSize);
    
    
    void getInputAddress(uint32_t* addressPtr) {
        
        if ((is_number(&asm_console_string[2]) == 1) & 
            (is_number(&asm_console_string[3]) == 1) & 
            (is_number(&asm_console_string[4]) == 1) & 
            (is_number(&asm_console_string[5]) == 1)) {
            
            uint8_t address[4];
            
            address[0] = asm_console_string[2];
            address[1] = asm_console_string[3];
            address[2] = asm_console_string[4];
            address[3] = asm_console_string[5];
            
            *addressPtr = string_get_int_long(address);
        }
        
        if ((is_number(&asm_console_string[2]) == 1) & 
            (is_number(&asm_console_string[3]) == 1) & 
            (is_number(&asm_console_string[4]) == 1) & 
            (is_number(&asm_console_string[5]) == 0)) {
            
            uint8_t address[4];
            
            address[0] = '0';
            address[1] = asm_console_string[2];
            address[2] = asm_console_string[3];
            address[3] = asm_console_string[4];
            
            *addressPtr = string_get_int_long(address);
        }
        
        if ((is_number(&asm_console_string[2]) == 1) & 
            (is_number(&asm_console_string[3]) == 1) & 
            (is_number(&asm_console_string[4]) == 0) & 
            (is_number(&asm_console_string[5]) == 0)) {
            
            uint8_t address[4];
            
            address[0] = '0';
            address[1] = '0';
            address[2] = asm_console_string[2];
            address[3] = asm_console_string[3];
            
            *addressPtr = string_get_int_long(address);
        }
        
        if ((is_number(&asm_console_string[2]) == 1) & 
            (is_number(&asm_console_string[3]) == 0) & 
            (is_number(&asm_console_string[4]) == 0) & 
            (is_number(&asm_console_string[5]) == 0)) {
            
            uint8_t address[4];
            
            address[0] = '0';
            address[1] = '0';
            address[2] = '0';
            address[3] = asm_console_string[2];
            
            *addressPtr = string_get_int_long(address);
        }
        
        asm_console_string[5] = ' ';
        asm_console_string[4] = ' ';
        asm_console_string[3] = ' ';
        asm_console_string[2] = ' ';
        
        return;
    }
    
    
    void printAddressLine(uint32_t addressPtr) {
        
        uint8_t assemblyAddressString[4];
        
        assemblyAddressString[0] = ' ';
        assemblyAddressString[1] = ' ';
        assemblyAddressString[2] = ' ';
        assemblyAddressString[3] = ' ';
        
        int_to_string(addressPtr, assemblyAddressString);
        
        uint8_t assemblyAddressStringCorrected[4];
        
        assemblyAddressStringCorrected[0] = assemblyAddressString[0];
        assemblyAddressStringCorrected[1] = assemblyAddressString[1];
        assemblyAddressStringCorrected[2] = assemblyAddressString[2];
        assemblyAddressStringCorrected[3] = assemblyAddressString[3];
        
        for (uint8_t i=0; i < 4; i++) {
            if (assemblyAddressStringCorrected[0] == ' ') {
                assemblyAddressStringCorrected[0] = assemblyAddressStringCorrected[1];
                assemblyAddressStringCorrected[1] = assemblyAddressStringCorrected[2];
                assemblyAddressStringCorrected[2] = assemblyAddressStringCorrected[3];
                assemblyAddressStringCorrected[3] = '0';
            }
        }
        
        for (uint8_t i=0; i < 4; i++) {
            if (assemblyAddressStringCorrected[3] == ' ') {
                assemblyAddressStringCorrected[3] = assemblyAddressStringCorrected[2];
                assemblyAddressStringCorrected[2] = assemblyAddressStringCorrected[1];
                assemblyAddressStringCorrected[1] = assemblyAddressStringCorrected[0];
                assemblyAddressStringCorrected[0] = '0';
            }
        }
        
        print(assemblyAddressStringCorrected, 5);
        printSpace(1);
        
        console_line = ConsoleGetCursorLine();
        
        console_position = 5;
        ConsoleSetCursor(console_line, console_position);
        
        return;
    }
    
    
    
    
    print(msgPromptString, sizeof(msgPromptString));
    
    //
    // Application entry point
    //
    
    ConsoleCursorDisable();
    
    while(1) {
        
        uint8_t currentChar = ConsoleGetRawChar();
        
        if (lastChar == currentChar) 
            continue;
        
        lastChar = currentChar;
        
        // Return
        if (lastChar == 0x02) {
            
            printLn();
            
            if (asm_console_string_length == 0) 
                assemblyState = 0;
            
            if (assemblyState == 0) {
                
                //
                // Quit
                //
                
                if ((asm_console_string[0] == 'q') & (asm_console_string[1] == ' ')) {
                    
                    fsFileClose();
                    
                    ConsoleCursorEnable();
                    
                    return;
                }
                
                //
                // Run the program in memory
                //
                
                if ((asm_console_string[0] == 'r') & (asm_console_string[1] == ' ')) {
                    
                    EmulatorSetProgram( fileBuffer, fileSize );
                    
                    EmulateX4(EVENT_INITIATE);
                    
                }
                
                
                //
                // Set the program size register
                //
                
                if ((asm_console_string[0] == 'r') & (asm_console_string[1] == 'c') & (asm_console_string[2] == 'x')) {
                    
                    if ((asm_console_string[4] != ' ') | (asm_console_string[5] != ' ') | (asm_console_string[6] != ' ') | (asm_console_string[7] != ' ')) {
                        
                        uint8_t newFileSize[4];
                        newFileSize[0] = asm_console_string[4];
                        newFileSize[1] = asm_console_string[5];
                        newFileSize[2] = asm_console_string[6];
                        newFileSize[3] = asm_console_string[7];
                        
                        if (asm_console_string[7] == ' ') {
                            newFileSize[3] = newFileSize[2];
                            newFileSize[2] = newFileSize[1];
                            newFileSize[1] = newFileSize[0];
                            newFileSize[0] = ' ';
                        }
                        
                        if (asm_console_string[6] == ' ') {
                            newFileSize[3] = newFileSize[2];
                            newFileSize[2] = newFileSize[1];
                            newFileSize[1] = newFileSize[0];
                            newFileSize[0] = ' ';
                        }
                        
                        if (asm_console_string[5] == ' ') {
                            newFileSize[3] = newFileSize[2];
                            newFileSize[2] = newFileSize[1];
                            newFileSize[1] = newFileSize[0];
                            newFileSize[0] = ' ';
                        }
                        
                        fileSize = string_get_int_long(newFileSize);
                    }
                    
                    uint8_t msgByte[] = "RCX: ";
                    print(msgByte, sizeof(msgByte));
                    
                    printInt( fileSize );
                    printLn();
                    
                }
                
                
                //
                // Write data to file
                //
                
                if ((asm_console_string[0] == 'w') & (asm_console_string[1] == ' ')) {
                    
                    // Delete the original
                    fsFileClose();
                    fileAddress = fsFileDelete(param, param_length);
                    
                    fsDirectoryRemoveFile( fsWorkingDirectoryGetAddress(), fileAddress );
                    
                    // Create new file with new size
                    fileAddress = fsFileCreate(param, param_length, fileSize);
                    
                    fsDirectoryAddFile( fsWorkingDirectoryGetAddress(), fileAddress );
                    
                    fsFileOpen(fileAddress);
                    
                    fsFileWrite(fileBuffer, fileSize);
                    printInt( fileSize );
                    
                    if (fileSize == 1) {
                        uint8_t msgByte[] = " byte ";
                        print(msgByte, sizeof(msgByte));
                    } else {
                        uint8_t msgBytes[] = " bytes ";
                        print(msgBytes, sizeof(msgBytes));
                    }
                    
                    uint8_t msgWritten[] = "written";
                    print(msgWritten, sizeof(msgWritten));
                    
                    printLn();
                    
                }
                
                
                //
                // Assemble
                //
                
                if (asm_console_string[0] == 'a') {
                    asm_console_string[0] = ' ';
                    
                    getInputAddress(&assemblyAddress);
                    
                    assemblyState = 1;
                    
                    printAddressLine(assemblyAddress);
                    
                    asm_console_string_length = 0;
                    
                    continue;
                }
                
                
                //
                // Disassemble
                //
                
                if (asm_console_string[0] == 'u') {
                    asm_console_string[0] = ' ';
                    
                    getInputAddress(&disassemblyAddress);
                    
                    // Print the lines
                    for (uint8_t i=0; i < 7; i++) {
                        
                        printAddressLine(disassemblyAddress);
                        
                        asm_console_string_length = 0;
                        
                        // Check end of file
                        if (disassemblyAddress >= fileSize) {
                            
                            uint8_t msgEndOfFile[] = "[EOF]";
                            print(msgEndOfFile, sizeof(msgEndOfFile));
                            
                            printLn();
                            
                            break;
                        }
                        
                        //
                        // Interpret machine code into op-codes
                        //
                        
                        uint8_t opCodeWasFound = 0;
                        uint8_t specialOpcodeArgs = 0;
                        
                        if (fileBuffer[disassemblyAddress] == 0x90) {printc("nop", 4);  opCodeWasFound = 1;}
                        if (fileBuffer[disassemblyAddress] == 0xFD) {printc("inc", 4);  opCodeWasFound = 2;}
                        if (fileBuffer[disassemblyAddress] == 0xFC) {printc("dec", 4);  opCodeWasFound = 2;}
                        
                        if (fileBuffer[disassemblyAddress] == 0x86) {printc("lda", 4);  opCodeWasFound = 5;}
                        if (fileBuffer[disassemblyAddress] == 0x87) {printc("sta", 4);  opCodeWasFound = 5;}
                        
                        if (fileBuffer[disassemblyAddress] == 0x85) {printc("in", 3);   opCodeWasFound = 5;}
                        if (fileBuffer[disassemblyAddress] == 0x84) {printc("out", 4);  opCodeWasFound = 5;}
                        
                        if (fileBuffer[disassemblyAddress] == 0x88) {printc("mov", 4);  opCodeWasFound = 3;}
                        if (fileBuffer[disassemblyAddress] == 0x89) {printc("mov", 4);  opCodeWasFound = 3; specialOpcodeArgs = 2;}
                        
                        if (fileBuffer[disassemblyAddress] == 0x00) {printc("add", 4);  opCodeWasFound = 1;}
                        if (fileBuffer[disassemblyAddress] == 0x80) {printc("sub", 4);  opCodeWasFound = 1;}
                        if (fileBuffer[disassemblyAddress] == 0xF6) {printc("mul", 4);  opCodeWasFound = 1;}
                        if (fileBuffer[disassemblyAddress] == 0xF4) {printc("div", 4);  opCodeWasFound = 1;}
                        
                        if (fileBuffer[disassemblyAddress] == 0x0F) {printc("pop", 4);  opCodeWasFound = 2;}
                        if (fileBuffer[disassemblyAddress] == 0xF0) {printc("push", 5); opCodeWasFound = 2;}
                        
                        if (fileBuffer[disassemblyAddress] == 0xFE) {printc("jmp", 4);  opCodeWasFound = 5;}
                        if (fileBuffer[disassemblyAddress] == 0x9A) {printc("call", 5);  opCodeWasFound = 5;}
                        
                        if (fileBuffer[disassemblyAddress] == 0xCB) {printc("ret", 4);  opCodeWasFound = 1;}
                        
                        if ((fileBuffer[disassemblyAddress] == 0xCC) | 
                            (fileBuffer[disassemblyAddress] == 0xCD)) {printc("int", 4);  opCodeWasFound = 2; specialOpcodeArgs = 1;}
                        
                        if (fileBuffer[disassemblyAddress] == 0xFA) {printc("cli", 4);  opCodeWasFound = 1;}
                        if (fileBuffer[disassemblyAddress] == 0xFB) {printc("sti", 4);  opCodeWasFound = 1;}
                        
                        // Unknown op-code
                        if (opCodeWasFound == 0) {
                            
                            printc("???", 4);
                            
                            disassemblyAddress++;
                            
                        } else {
                            
                            disassemblyAddress += opCodeWasFound;
                            
                            //
                            // Print the opcode arguments
                            
                            
                            if (opCodeWasFound == 2) {
                                
                                uint8_t argA = fileBuffer[disassemblyAddress - 1];
                                
                                uint8_t argumentChar[2] = {'a', 'x'};
                                
                                
                                printSpace(1);
                                
                                if (specialOpcodeArgs == 1) {
                                    
                                    int_to_hex_string(argA, argumentChar);
                                    
                                    print(argumentChar, 3);
                                    printChar('h');
                                    
                                } else {
                                    
                                    argumentChar[0] += argA;
                                    
                                    if (argumentChar[0] == 'a') {argumentChar[0] = 'a'; argumentChar[1] = 'l';}
                                    if (argumentChar[0] == 'b') {argumentChar[0] = 'a'; argumentChar[1] = 'h';}
                                    if (argumentChar[0] == 'c') {argumentChar[0] = 'b'; argumentChar[1] = 'l';}
                                    if (argumentChar[0] == 'd') {argumentChar[0] = 'b'; argumentChar[1] = 'h';}
                                    if (argumentChar[0] == 'e') {argumentChar[0] = 'c'; argumentChar[1] = 'l';}
                                    if (argumentChar[0] == 'f') {argumentChar[0] = 'c'; argumentChar[1] = 'h';}
                                    if (argumentChar[0] == 'g') {argumentChar[0] = 'd'; argumentChar[1] = 'l';}
                                    if (argumentChar[0] == 'h') {argumentChar[0] = 'd'; argumentChar[1] = 'h';}
                                    
                                    
                                    print(argumentChar, 3);
                                    
                                }
                                
                            }
                            
                            
                            //
                            // Argument A is register
                            // Argument B is register or hex
                            
                            if (opCodeWasFound == 3) {
                                
                                uint8_t argA = fileBuffer[disassemblyAddress - 2];
                                uint8_t argB = fileBuffer[disassemblyAddress - 1];
                                
                                uint8_t argumentCharA[2] = {'a', 'x'};
                                uint8_t argumentCharB[2] = {'a', 'x'};
                                
                                argumentCharA[0] += argA;
                                
                                if (argumentCharA[0] == 'a') {argumentCharA[0] = 'a'; argumentCharA[1] = 'l';}
                                if (argumentCharA[0] == 'b') {argumentCharA[0] = 'a'; argumentCharA[1] = 'h';}
                                if (argumentCharA[0] == 'c') {argumentCharA[0] = 'b'; argumentCharA[1] = 'l';}
                                if (argumentCharA[0] == 'd') {argumentCharA[0] = 'b'; argumentCharA[1] = 'h';}
                                if (argumentCharA[0] == 'e') {argumentCharA[0] = 'c'; argumentCharA[1] = 'l';}
                                if (argumentCharA[0] == 'f') {argumentCharA[0] = 'c'; argumentCharA[1] = 'h';}
                                if (argumentCharA[0] == 'g') {argumentCharA[0] = 'd'; argumentCharA[1] = 'l';}
                                if (argumentCharA[0] == 'h') {argumentCharA[0] = 'd'; argumentCharA[1] = 'h';}
                                
                                printSpace(1);
                                print(argumentCharA, 3);
                                printc(",", 2);
                                
                                // Move byte / register
                                if (specialOpcodeArgs == 2) {
                                    
                                    int_to_hex_string(argB, argumentCharB);
                                    
                                    print(argumentCharB, 3);
                                    printChar('h');
                                    
                                }
                                
                                // Move register to register
                                if (specialOpcodeArgs == 0) {
                                    
                                    argumentCharB[0] += argB;
                                    
                                    if (argumentCharB[0] == 'a') {argumentCharB[0] = 'a'; argumentCharB[1] = 'l';}
                                    if (argumentCharB[0] == 'b') {argumentCharB[0] = 'a'; argumentCharB[1] = 'h';}
                                    if (argumentCharB[0] == 'c') {argumentCharB[0] = 'b'; argumentCharB[1] = 'l';}
                                    if (argumentCharB[0] == 'd') {argumentCharB[0] = 'b'; argumentCharB[1] = 'h';}
                                    if (argumentCharB[0] == 'e') {argumentCharB[0] = 'c'; argumentCharB[1] = 'l';}
                                    if (argumentCharB[0] == 'f') {argumentCharB[0] = 'c'; argumentCharB[1] = 'h';}
                                    if (argumentCharB[0] == 'g') {argumentCharB[0] = 'd'; argumentCharB[1] = 'l';}
                                    if (argumentCharB[0] == 'h') {argumentCharB[0] = 'd'; argumentCharB[1] = 'h';}
                                    
                                    print(argumentCharB, 3);
                                }
                                
                            }
                            
                            //
                            // Argument A is address
                            
                            if (opCodeWasFound == 5) {
                                
                                uint8_t argA[4];
                                argA[3] = fileBuffer[disassemblyAddress - 4];
                                argA[2] = fileBuffer[disassemblyAddress - 3];
                                argA[1] = fileBuffer[disassemblyAddress - 2];
                                argA[0] = fileBuffer[disassemblyAddress - 1];
                                
                                uint8_t hexA[2] = {'0', '0'};
                                uint8_t hexB[2] = {'0', '0'};
                                uint8_t hexC[2] = {'0', '0'};
                                uint8_t hexD[2] = {'0', '0'};
                                
                                int_to_hex_string(argA[0], hexA);
                                int_to_hex_string(argA[1], hexB);
                                int_to_hex_string(argA[2], hexC);
                                int_to_hex_string(argA[3], hexD);
                                
                                printSpace(1);
                                
                                print(hexA, 3);
                                print(hexB, 3);
                                print(hexC, 3);
                                print(hexD, 3);
                                
                                printChar('h');
                                
                            }
                            
                        }
                        
                        printLn();
                        
                    }
                    
                }
                
                
                //
                // Dump
                //
                
                if (asm_console_string[0] == 'd') {
                    asm_console_string[0] = ' ';
                    
                    for (uint8_t l=0; l < 7; l++) {
                        
                        getInputAddress(&dumpAddress);
                        
                        uint8_t hexString[17];
                        
                        hexString[8] = '-';
                        
                        for (uint8_t i=0; i < 4; i++) int_to_hex_string( fileBuffer[i +      dumpAddress], &hexString[ i * 2] );
                        for (uint8_t i=0; i < 4; i++) int_to_hex_string( fileBuffer[i + 4  + dumpAddress], &hexString[(i * 2) + 9] );
                        
                        dumpAddress += 8;
                        
                        print(hexString, sizeof(hexString)+1);
                        printLn();
                        
                    }
                    */
                    
                    /*
                    
                        getInputAddress(&dumpAddress);
                        
                        uint8_t hexStringA[17];
                        uint8_t hexStringB[17];
                        uint8_t hexStringC[17];
                        
                        hexStringA[8] = '-';
                        hexStringB[8] = '-';
                        hexStringC[8] = '-';
                        
                        for (uint8_t i=0; i < 4; i++) int_to_hex_string( fileBuffer[i + dumpAddress], &hexStringA[i * 2] );
                        for (uint8_t i=0; i < 4; i++) int_to_hex_string( fileBuffer[i + 4  + dumpAddress], &hexStringA[(i * 2) + 9] );
                        for (uint8_t i=0; i < 4; i++) int_to_hex_string( fileBuffer[i + 8  + dumpAddress], &hexStringB[(i * 2)] );
                        for (uint8_t i=0; i < 4; i++) int_to_hex_string( fileBuffer[i + 12 + dumpAddress], &hexStringB[(i * 2) + 9] );
                        for (uint8_t i=0; i < 4; i++) int_to_hex_string( fileBuffer[i + 16 + dumpAddress], &hexStringC[(i * 2)] );
                        for (uint8_t i=0; i < 4; i++) int_to_hex_string( fileBuffer[i + 20 + dumpAddress], &hexStringC[(i * 2) + 9] );
                        
                        dumpAddress += 24;
                        
                        print(hexStringA, sizeof(hexStringA)+1);
                        printLn();
                        print(hexStringB, sizeof(hexStringB)+1);
                        printLn();
                        print(hexStringC, sizeof(hexStringC)+1);
                        printLn();
                        
                    */
                
                /*
                }
                
                
                //
                // Print the prompt line
                
                print(msgPromptString, sizeof(msgPromptString));
                
                console_position = 1;
                console_line = ConsoleGetCursorLine();
                ConsoleSetCursor(console_line, console_position);
                
                // Clear the old op codes
                for (uint8_t i=0; i < 40; i++) 
                    asm_console_string[i] = ' ';
                
                asm_console_string_length = 0;
                
            } else {
                
                // Assembly prompt
                
                //
                // Op-code to machine code translation for disassembly
                //
                
                uint8_t opCode[4] = {asm_console_string[0], asm_console_string[1], asm_console_string[2], asm_console_string[3]};
                uint8_t argCount = 0;
                
                if ((opCode[0] == 'd') & (opCode[1] == 'b') & (opCode[2] == ' ') & (opCode[3] != ' ')) {
                    
                    for (uint8_t i=0; i < (asm_console_string_length - 3); i++) 
                        fileBuffer[assemblyAddress + i] = asm_console_string[3 + i];
                    
                    assemblyAddress += (asm_console_string_length - 3);
                    
                    // Clear the old op codes
                    for (uint8_t i; i < 40; i++) 
                        asm_console_string[i] = ' ';
                    
                    printAddressLine(assemblyAddress);
                    
                    asm_console_string_length = 0;
                    
                    // Clear the old op codes
                    for (uint8_t i=0; i < 4; i++) {
                        opCode[i] = ' ';
                        asm_console_string[i] = ' ';
                    }
                    
                    continue;
                }
                
                if ((opCode[0] == 'n') & (opCode[1] == 'o') & (opCode[2] == 'p')) {fileBuffer[assemblyAddress] = 0x90; assemblyAddress += 1; argCount = 0;}
                // IO
                if ((opCode[0] == 'i') & (opCode[1] == 'n') & (opCode[2] == ' ')) {fileBuffer[assemblyAddress] = 0x85; assemblyAddress += 5; argCount = 4;}
                if ((opCode[0] == 'o') & (opCode[1] == 'u') & (opCode[2] == 't')) {fileBuffer[assemblyAddress] = 0x84; assemblyAddress += 5; argCount = 4;}
                // Mem
                if ((opCode[0] == 'l') & (opCode[1] == 'd') & (opCode[2] == 'a')) {fileBuffer[assemblyAddress] = 0x86; assemblyAddress += 5; argCount = 4;}
                if ((opCode[0] == 's') & (opCode[1] == 't') & (opCode[2] == 'a')) {fileBuffer[assemblyAddress] = 0x87; assemblyAddress += 5; argCount = 4;}
                
                if ((opCode[0] == 'm') & (opCode[1] == 'o') & (opCode[2] == 'v')) {fileBuffer[assemblyAddress] = 0x88; assemblyAddress += 3; argCount = 2;}
                // Math
                if ((opCode[0] == 'i') & (opCode[1] == 'n') & (opCode[2] == 'c')) {fileBuffer[assemblyAddress] = 0xFD; assemblyAddress += 2; argCount = 1;}
                if ((opCode[0] == 'd') & (opCode[1] == 'e') & (opCode[2] == 'c')) {fileBuffer[assemblyAddress] = 0xFC; assemblyAddress += 2; argCount = 1;}
                
                if ((opCode[0] == 'a') & (opCode[1] == 'd') & (opCode[2] == 'd')) {fileBuffer[assemblyAddress] = 0x00; assemblyAddress += 1; argCount = 0;}
                if ((opCode[0] == 's') & (opCode[1] == 'u') & (opCode[2] == 'b')) {fileBuffer[assemblyAddress] = 0x80; assemblyAddress += 1; argCount = 0;}
                if ((opCode[0] == 'm') & (opCode[1] == 'u') & (opCode[2] == 'l')) {fileBuffer[assemblyAddress] = 0xF6; assemblyAddress += 1; argCount = 0;}
                if ((opCode[0] == 'd') & (opCode[1] == 'i') & (opCode[2] == 'v')) {fileBuffer[assemblyAddress] = 0xF4; assemblyAddress += 1; argCount = 0;}
                // Stack
                if ((opCode[0] == 'p') & (opCode[1] == 'o') & (opCode[2] == 'p')) {fileBuffer[assemblyAddress] = 0x0F; assemblyAddress += 2; argCount = 1;}
                if ((opCode[0] == 'p') & (opCode[1] == 'u') & (opCode[2] == 's') & 
                                                              (opCode[3] == 'h')) {fileBuffer[assemblyAddress] = 0xF0; assemblyAddress += 2; argCount = 1;}
                // Compare
                if ((opCode[0] == 'c') & (opCode[1] == 'm') & (opCode[2] == 'p')) {fileBuffer[assemblyAddress] = 0x3C; assemblyAddress += 3; argCount = 2;}
                // Jump / calls
                if ((opCode[0] == 'j') & (opCode[1] == 'm') & (opCode[2] == 'p')) {fileBuffer[assemblyAddress] = 0xFE; assemblyAddress += 5; argCount = 4;}
                if ((opCode[0] == 'c') & (opCode[1] == 'a') & (opCode[2] == 'l') & 
                                                              (opCode[3] == 'l')) {fileBuffer[assemblyAddress] = 0x9A; assemblyAddress += 5; argCount = 4;}
                
                if ((opCode[0] == 'r') & (opCode[1] == 'e') & (opCode[2] == 't')) {fileBuffer[assemblyAddress] = 0xCB; assemblyAddress += 1; argCount = 0;}
                // Interrupts
                if ((opCode[0] == 'i') & (opCode[1] == 'n') & (opCode[2] == 't')) {fileBuffer[assemblyAddress] = 0xCC; assemblyAddress += 2; argCount = 1;}
                if ((opCode[0] == 'c') & (opCode[1] == 'l') & (opCode[2] == 'i')) {fileBuffer[assemblyAddress] = 0xFA; assemblyAddress += 1; argCount = 0;}
                if ((opCode[0] == 's') & (opCode[1] == 't') & (opCode[2] == 'i')) {fileBuffer[assemblyAddress] = 0xFB; assemblyAddress += 1; argCount = 0;}
                
                //
                // Gather arguments
                //
                
                uint8_t argA[4] = {0, 0, 0, 0};
                uint8_t argB[4] = {0, 0, 0, 0};
                uint8_t argAIsHex = 0;
                uint8_t argBIsHex = 0;
                
                // Arguments after a 4 digit opcode
                if ((is_letter(&asm_console_string[5]) == 1) & (is_letter(&asm_console_string[6]) == 1)) {
                    argA[0] = asm_console_string[5];
                    argA[1] = asm_console_string[6];
                }
                
                // 4 digit opcode with numeric arguments
                if ((is_number(&asm_console_string[5]) == 1) & (is_number(&asm_console_string[6]) == 1) & 
                    (is_number(&asm_console_string[7]) == 1) & (is_number(&asm_console_string[8]) == 1)) {
                    argA[0] = asm_console_string[5];
                    argA[1] = asm_console_string[6];
                    argA[2] = asm_console_string[7];
                    argA[3] = asm_console_string[8];
                }
                
                // Arguments after a 3 digit opcode
                if ((is_letter(&asm_console_string[4]) == 1) & (is_letter(&asm_console_string[5]) == 1)) {
                    argA[0] = asm_console_string[4];
                    argA[1] = asm_console_string[5];
                }
                
                if ((is_letter(&asm_console_string[7]) == 1) & (is_letter(&asm_console_string[8]) == 1)) {
                    argB[0] = asm_console_string[7];
                    argB[1] = asm_console_string[8];
                }
                
                // Check hex
                if ((is_hex(&asm_console_string[4]) == 1) & (is_hex(&asm_console_string[5]) == 1)) {
                    argA[0] = asm_console_string[4];
                    argA[1] = asm_console_string[5];
                    argAIsHex = 1;
                }
                
                if ((is_hex(&asm_console_string[7]) == 1) & (is_hex(&asm_console_string[8]) == 1)) {
                    argB[0] = asm_console_string[7];
                    argB[1] = asm_console_string[8];
                    argBIsHex = 1;
                }
                
                // Arguments after a 3 digit opcode with a numeric argument
                if ((is_number(&asm_console_string[4]) == 1) & 
                    (is_number(&asm_console_string[5]) == 1) & 
                    (is_number(&asm_console_string[6]) == 1) & 
                    (is_number(&asm_console_string[7]) == 1) & 
                    (is_number(&asm_console_string[8]) == 1) & 
                    (is_number(&asm_console_string[9]) == 1) & 
                    (is_number(&asm_console_string[10]) == 1) & 
                    (is_number(&asm_console_string[11]) == 1)) {
                    
                    argA[0] = asm_console_string[4];
                    argA[1] = asm_console_string[5];
                    argA[2] = asm_console_string[6];
                    argA[3] = asm_console_string[7];
                    
                    argB[0] = asm_console_string[8];
                    argB[1] = asm_console_string[9];
                    argB[2] = asm_console_string[10];
                    argB[3] = asm_console_string[11];
                    
                }
                
                
                //
                // Disassemble the opcode arguments
                //
                
                //
                // Set argument A
                
                if (argCount == 1) {
                    uint8_t regIndex = 0;
                    
                    if ((argA[0] == 'a') & (argA[1] == 'l')) {regIndex = 0;}
                    if ((argA[0] == 'a') & (argA[1] == 'h')) {regIndex = 1;}
                    if ((argA[0] == 'b') & (argA[1] == 'l')) {regIndex = 2;}
                    if ((argA[0] == 'b') & (argA[1] == 'h')) {regIndex = 3;}
                    if ((argA[0] == 'c') & (argA[1] == 'l')) {regIndex = 4;}
                    if ((argA[0] == 'c') & (argA[1] == 'h')) {regIndex = 5;}
                    if ((argA[0] == 'd') & (argA[1] == 'l')) {regIndex = 6;}
                    if ((argA[0] == 'd') & (argA[1] == 'h')) {regIndex = 7;}
                    
                    //
                    // Argument A is hex
                    
                    if (argAIsHex == 1) {
                        uint8_t hexValFlip[2] = {argA[1], argA[0]};
                        
                        uint8_t value = string_get_hex_char(hexValFlip);
                        
                        regIndex = value;
                    }
                    
                    fileBuffer[assemblyAddress - 1] = regIndex;
                    
                }
                
                
                //
                // Set argument A & B
                
                if (argCount == 2) {
                    uint8_t regIndexA = 0;
                    uint8_t regIndexB = 0;
                    
                    if ((argA[0] == 'a') & (argA[1] == 'l')) {regIndexA = 0;}
                    if ((argA[0] == 'a') & (argA[1] == 'h')) {regIndexA = 1;}
                    if ((argA[0] == 'b') & (argA[1] == 'l')) {regIndexA = 2;}
                    if ((argA[0] == 'b') & (argA[1] == 'h')) {regIndexA = 3;}
                    if ((argA[0] == 'c') & (argA[1] == 'l')) {regIndexA = 4;}
                    if ((argA[0] == 'c') & (argA[1] == 'h')) {regIndexA = 5;}
                    if ((argA[0] == 'd') & (argA[1] == 'l')) {regIndexA = 6;}
                    if ((argA[0] == 'd') & (argA[1] == 'h')) {regIndexA = 7;}
                    
                    if ((argB[0] == 'a') & (argB[1] == 'l')) {regIndexB = 0;}
                    if ((argB[0] == 'a') & (argB[1] == 'h')) {regIndexB = 1;}
                    if ((argB[0] == 'b') & (argB[1] == 'l')) {regIndexB = 2;}
                    if ((argB[0] == 'b') & (argB[1] == 'h')) {regIndexB = 3;}
                    if ((argB[0] == 'c') & (argB[1] == 'l')) {regIndexB = 4;}
                    if ((argB[0] == 'c') & (argB[1] == 'h')) {regIndexB = 5;}
                    if ((argB[0] == 'd') & (argB[1] == 'l')) {regIndexB = 6;}
                    if ((argB[0] == 'd') & (argB[1] == 'h')) {regIndexB = 7;}
                    
                    //
                    // Argument B is hex
                    
                    if (argBIsHex == 1) {
                        
                        // Move reg should be move byte
                        if (fileBuffer[assemblyAddress - 3] == 0x88) {
                            
                            fileBuffer[assemblyAddress - 3] = 0x89;
                            
                            uint8_t hexValFlip[2] = {argB[1], argB[0]};
                            
                            uint8_t value = string_get_hex_char(hexValFlip);
                            
                            regIndexB = value;
                            
                        }
                        
                    }
                    
                    fileBuffer[assemblyAddress - 2] = regIndexA;
                    fileBuffer[assemblyAddress - 1] = regIndexB;
                    
                }
                
                
                //
                // Set argument A as an address
                
                if (argCount == 4) {
                    
                    uint8_t hexValFlipA[2] = {argA[1], argA[0]};
                    uint8_t hexValFlipB[2] = {argA[3], argA[2]};
                    uint8_t hexValFlipC[2] = {argB[1], argB[0]};
                    uint8_t hexValFlipD[2] = {argB[3], argB[2]};
                    
                    fileBuffer[assemblyAddress - 4] = string_get_hex_char(hexValFlipD);
                    fileBuffer[assemblyAddress - 3] = string_get_hex_char(hexValFlipC);
                    fileBuffer[assemblyAddress - 2] = string_get_hex_char(hexValFlipB);
                    fileBuffer[assemblyAddress - 1] = string_get_hex_char(hexValFlipA);
                    
                }
                
                
                // Clear the old op codes
                for (uint8_t i=0; i < 4; i++) 
                    opCode[i] = ' ';
                
                for (uint8_t i; i < 40; i++) 
                    asm_console_string[i] = ' ';
                
                printAddressLine(assemblyAddress);
                
                asm_console_string_length = 0;
                
            }
            
            // Clear the old op codes
            for (uint8_t i; i < 40; i++) 
                asm_console_string[i] = ' ';
            
            asm_console_string_length = 0;
            
            continue;
        }
        
        // Backspace
        if (lastChar == 0x01) {
            
            if (asm_console_string_length > 0) {
                
                if (assemblyState == 0) {
                    ConsoleSetCursorPosition(asm_console_string_length);
                } else {
                    ConsoleSetCursorPosition(asm_console_string_length + 4);
                }
                
                printChar(' ');
                asm_console_string[asm_console_string_length] = ' ';
                
                ConsoleSetCursorPosition(asm_console_string_length);
                
                asm_console_string_length--;
                
            }
            
            // Check to clear the old op codes
            if (asm_console_string_length == 0) {
                
                for (uint8_t i=0; i < 4; i++) 
                    asm_console_string[i] = ' ';
                
            }
            
            
            if (assemblyState == 0) {
                
                ConsoleSetCursorPosition(1);
                
            } else {
                
                ConsoleSetCursorPosition(5);
                
            }
            
            print(asm_console_string, asm_console_string_length + 1);
            
            continue;
        }
        
        // Escape exit
        if (lastChar == 0x07) {
            
            fsFileClose();
            
            return;
        }
        
        // Place a character
        if (lastChar > 0x19) {
            
            asm_console_string[asm_console_string_length] = lastChar;
            asm_console_string_length++;
            
            if (assemblyState == 0) {
                ConsoleSetCursorPosition(1);
            } else {
                ConsoleSetCursorPosition(5);
            }
            
            print(asm_console_string, asm_console_string_length + 1);
            
        }
        
        continue;
    }
    
    fsFileClose();
    
    return;
    */
    
}


void registerCommandAssembly(void) {
    
    uint8_t asmCommandName[] = "asm";
    
    ConsoleRegisterCommand(asmCommandName, sizeof(asmCommandName), functionAsm);
    
    return;
}

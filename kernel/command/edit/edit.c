#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/command/edit/edit.h>

void functionEDIT(uint8_t* param, uint8_t param_length) {
    
    /*
    
    uint8_t msgFileNotFoundEdittor[] = "File not found";
    uint8_t msgSaved[]               = "Saved";
    uint8_t msgSaveQuit[]            = "(S)ave / (Q)uit?";
    
    // File state
    uint8_t filename[10];
    uint8_t filename_length = 0;
    
    if (param_length < 3) 
        return;
    
    for (uint8_t i=0; i < param_length+1; i++) {
        
        if (param[i] == ' ') {
            filename_length = i;
            break;
        }
        
        filename[i] = param[i];
    }
    
    if (is_letter(&filename[0]) == 0) 
        return;
    
    if (filename[0] == ' ') 
        return;
    
    if (filename_length < 1) 
        return;
    
    uint8_t enableLineEnding = 0;
    
    uint8_t activeLines = 0;
    
    // Cursor state
    uint8_t lastChar = ConsoleGetLastChar();
    uint8_t cursorPos  = 0;
    uint8_t cursorLine = 0;
    
    // Open file
    uint32_t fileAddressBegin = fsFileExists(filename, filename_length);
    
    fsFileOpen(fileAddressBegin);
    
    uint32_t fileSize = fsFileGetSize(fileAddressBegin);
    
    if (fileAddressBegin == 0) {
        
        print( msgFileNotFoundEdittor, sizeof(msgFileNotFoundEdittor) );
        printLn();
        
        return;
    }
    
    // Editor buffer
    uint8_t textLineA[30];
    uint8_t textLineB[30];
    uint8_t textLineC[30];
    uint8_t textLineD[30];
    uint8_t textLineE[30];
    uint8_t textLineF[30];
    uint8_t textLineG[30];
    uint8_t textLineH[30];
    
    for (uint8_t i=0; i < 21; i++) {
        textLineA[i] = ' ';
        textLineB[i] = ' ';
        textLineC[i] = ' ';
        textLineD[i] = ' ';
        textLineE[i] = ' ';
        textLineF[i] = ' ';
        textLineG[i] = ' ';
        textLineH[i] = ' ';
    }
    
    uint8_t textBuffer[fileSize];
    for (uint16_t i=0; i < fileSize; i++) 
        textBuffer[i] = ' ';
    
    // Load the file
    fsFileReadText(textBuffer, fileSize);
    
    uint8_t doUpdateFrame  = 1;
    uint8_t doLoadPage     = 1;
    uint16_t pageNumber    = 0;
    uint16_t pageOffset    = 0;
    
    uint8_t line           = 0;
    uint8_t position       = 0;
    
    //uint8_t flagEOF   = 0;
    
    uint8_t shiftState = 0;
    
    ConsoleClearScreen();
    ConsoleSetCursor(0, 0);
    
    uint8_t isActive = 1;
    while (isActive) {
        
        //
        // Page loader
        
        if (doLoadPage == 1) {
            
            doLoadPage = 0;
            
            line       = 0;
            position   = 0;
            
            activeLines = 0;
            
            uint16_t pageCount = 0;
            
            
            //
            // Get the current page offset
            
            if (pageNumber > 0) {
                for (uint16_t i=0; i < fileSize; i++) {
                    
                    // Check end of file
                    if (i < fileSize-1) {
                        
                        //flagEOF = 0;
                        
                    } else {
                        
                        //flagEOF = 1;
                    }
                    
                    if (textBuffer[i] != '\n') 
                        continue;
                    
                    if (pageCount == pageNumber) {
                        
                        pageOffset = i + 1;
                        
                        break;
                        
                    } else {
                        
                        if (i < fileSize-1) {
                            
                            pageCount++;
                            
                        }
                        
                    }
                    
                }
                
            } else {
                
                pageOffset = 0;
                
            }
            
            // Split file text into lines
            for (uint16_t i=pageOffset; i < fileSize; i++) {
                
                if (line > 7) 
                    break;
                
                uint8_t* textLine = textLineA;
                if (line == 0) textLine = textLineA;
                if (line == 1) textLine = textLineB;
                if (line == 2) textLine = textLineC;
                if (line == 3) textLine = textLineD;
                if (line == 4) textLine = textLineE;
                if (line == 5) textLine = textLineF;
                if (line == 6) textLine = textLineG;
                if (line == 7) textLine = textLineH;
                
                textLine[position] = textBuffer[i];
                
                if (textBuffer[i] == '\n') {
                    
                    textLine[position] = '\n';
                    
                    // New line
                    
                    line++;
                    activeLines++;
                    
                    position = 0;
                    
                } else {
                    
                    if (textBuffer[i] == '\0') {
                        
                        textLine[position] = '\n';
                        
                        continue;
                    }
                    
                    // Add a character to the line
                    
                    position++;
                    
                    if (position > 19) {
                        
                        line++;
                        
                        position = 0;
                    }
                    
                }
            }
            
            // Check line count
            if (activeLines == 7) {
                textLineH[0] = '\n';
            }
            
            if (activeLines == 6) {
                textLineH[0] = '\n';
                textLineG[0] = '\n';
            }
            
            if (activeLines == 5) {
                textLineH[0] = '\n';
                textLineG[0] = '\n';
                textLineF[0] = '\n';
            }
            
            if (activeLines == 4) {
                textLineH[0] = '\n';
                textLineG[0] = '\n';
                textLineF[0] = '\n';
                textLineE[0] = '\n';
            }
            
            if (activeLines == 3) {
                textLineH[0] = '\n';
                textLineG[0] = '\n';
                textLineF[0] = '\n';
                textLineE[0] = '\n';
                textLineD[0] = '\n';
            }
            
            if (activeLines == 2) {
                textLineH[0] = '\n';
                textLineG[0] = '\n';
                textLineF[0] = '\n';
                textLineE[0] = '\n';
                textLineD[0] = '\n';
                textLineC[0] = '\n';
            }
            
            if (activeLines == 1) {
                textLineH[0] = '\n';
                textLineG[0] = '\n';
                textLineF[0] = '\n';
                textLineE[0] = '\n';
                textLineD[0] = '\n';
                textLineC[0] = '\n';
                textLineB[0] = '\n';
            }
            
            if (activeLines == 0) {
                textLineH[0] = '\n';
                textLineG[0] = '\n';
                textLineF[0] = '\n';
                textLineE[0] = '\n';
                textLineD[0] = '\n';
                textLineC[0] = '\n';
                textLineB[0] = '\n';
                textLineA[0] = '\n';
            }
            
            activeLines = 8;
        }
        
        
        
        
        //
        // Draw the text buffer
        if (doUpdateFrame == 1) {
            
            doUpdateFrame = 0;
            
            ConsoleSetCursor(0, 0);
            
            for (uint8_t i=0; i < 8; i++) {
                uint8_t* textLine = textLineA;
                
                if (i == 0) textLine = textLineA;
                if (i == 1) textLine = textLineB;
                if (i == 2) textLine = textLineC;
                if (i == 3) textLine = textLineD;
                if (i == 4) textLine = textLineE;
                if (i == 5) textLine = textLineF;
                if (i == 6) textLine = textLineG;
                if (i == 7) textLine = textLineH;
                
                for (uint8_t p=0; p < 20; p++) {
                    
                    if ((textLine[p] == '\0') | (textLine[p] == '\n')) {
                        
                        if (enableLineEnding == 1) {
                            if (p < 19) {
                                
                                if (textLine[p] == '\n') printChar('<');
                                if (textLine[p] == '\0') printChar('#');
                                
                            }
                        }
                        
                        printSpace(20 - p);
                        
                        break;
                    }
                    
                    printChar( textLine[p] );
                }
                
                if (i < 7) printLn();
                
            }
            
            ConsoleSetCursor(cursorLine, cursorPos);
        }
        
        uint8_t currentChar = ConsoleGetRawChar();
        
        // Shift UPPERCASE
        if (shiftState == 1) {
            
            if (is_letter(&currentChar) == 1) {
                
                currentChar -= 0x20;
                
            } else {
                
                if ((currentChar == '[') | (currentChar == ']')) currentChar += 0x20;
                
                if (currentChar == 0x3B) currentChar--;
                
                if (currentChar == '-')  currentChar = '_';
                if (currentChar == '=')  currentChar = '+';
                if (currentChar == '\\') currentChar = '|';
                if (currentChar == 0x27) currentChar = 0x22;
                if (currentChar == ',')  currentChar = '<';
                if (currentChar == '.')  currentChar = '>';
                if (currentChar == '/')  currentChar = '?';
                
            }
            
            if (is_number(&currentChar) == 1) {
                
                if (currentChar == '0') currentChar = ')';
                if (currentChar == '1') currentChar = '!';
                if (currentChar == '2') currentChar = '@';
                if (currentChar == '3') currentChar = '#';
                if (currentChar == '4') currentChar = '$';
                if (currentChar == '5') currentChar = '%';
                if (currentChar == '6') currentChar = '^';
                if (currentChar == '7') currentChar = '&';
                if (currentChar == '8') currentChar = '*';
                if (currentChar == '9') currentChar = '(';
                
            }
            
        }
        
        // Check incoming character
        if (lastChar == currentChar) 
            continue;
        
        lastChar = currentChar;
        
        
        
        // Page up
        */
        
        /*
        if (lastChar == 0xF5) {
            
            doLoadPage = 1;
            doUpdateFrame = 1;
            
            if (pageNumber > 0) 
                pageNumber--;
            
        }
        
        // Page down
        if (lastChar == 0xF6) {
            
            doLoadPage = 1;
            doUpdateFrame = 1;
            
            if (flagEOF == 0) 
                if (pageNumber < 20) 
                    pageNumber++;
            
        }
        */
        
        
        
        
        
        /*
        // Toggle line ending characters
        if (lastChar == 0xF1) {
            
            enableLineEnding = !enableLineEnding;
            
            doUpdateFrame = 1;
        }
        
        // Cursor down
        if (lastChar == 0x04) {
            
            if (((cursorLine+1) < activeLines) & (cursorLine < 7)) 
                cursorLine++;
            
            doUpdateFrame = 1;
        }
        
        // Cursor left
        if (lastChar == 0x05) {
            if (cursorPos > 0) {
                cursorPos--;
            } else {
                if (cursorLine > 0) {
                    cursorPos = 19;
                    cursorLine--;
                }
            }
            doUpdateFrame = 1;
        }
        
        // Cursor up
        if (lastChar == 0x03) {
            if (cursorLine > 0) 
                cursorLine--;
            doUpdateFrame = 1;
        }
        
        // Cursor right
        if (lastChar == 0x06) {
            if (cursorPos < 19) {
                cursorPos++;
            } else {
                if (cursorLine < 8) {
                    cursorPos = 0;
                    cursorLine++;
                }
            }
            doUpdateFrame = 1;
        }
        
        // Shift pressed
        if (lastChar == 0x11) {
            
            shiftState = 1;
        }
        
        // Shift released
        if (lastChar == 0x12) {
            
            shiftState = 0;
        }
        
        // Check cursor EOL
        uint8_t lineEOL = 0;
        
        uint8_t* textLineEOL = textLineA;
        if (cursorLine == 0) textLineEOL = textLineA;
        if (cursorLine == 1) textLineEOL = textLineB;
        if (cursorLine == 2) textLineEOL = textLineC;
        if (cursorLine == 3) textLineEOL = textLineD;
        if (cursorLine == 4) textLineEOL = textLineE;
        if (cursorLine == 5) textLineEOL = textLineF;
        if (cursorLine == 6) textLineEOL = textLineG;
        if (cursorLine == 7) textLineEOL = textLineH;
        
        for (uint8_t i=0; i < 21; i++) {
            
            if (textLineEOL[i] == '\n') {
                lineEOL = i;
                break;
            }
            
            if (i == 20) 
                textLineEOL[0] = '\n';
            
        }
        
        if (cursorPos > lineEOL) cursorPos = lineEOL;
        
        
        //
        // Backspace
        
        if ((lastChar == 0x10) | (lastChar == 0x01)) {
            
            // Shift back the text line
            uint8_t* textLine = 0;
            if (cursorLine == 0) textLine = textLineA;
            if (cursorLine == 1) textLine = textLineB;
            if (cursorLine == 2) textLine = textLineC;
            if (cursorLine == 3) textLine = textLineD;
            if (cursorLine == 4) textLine = textLineE;
            if (cursorLine == 5) textLine = textLineF;
            if (cursorLine == 6) textLine = textLineG;
            if (cursorLine == 7) textLine = textLineH;
            
            if (textLine[0] != '\n') {
                
                textLine[21] = ' ';
                
                // Find EOL
                uint8_t posEOL = 0;
                
                if ((cursorPos > 0) | (lastChar == 0x10)) {
                    
                    // Non backspacing
                    if (lastChar != 0x10) 
                        cursorPos--;
                    
                    for (uint8_t i=0; i < 21; i++) {
                        
                        posEOL = i;
                        
                        if (textLine[i] == '\n') 
                            break;
                    }
                    
                    for (uint8_t i=cursorPos; i < posEOL; i++) {
                        textLine[i] = textLine[i + 1];
                        
                    }
                    
                }
                
            }
            
            doUpdateFrame = 1;
        }
        
        
        //
        // Place character
        
        if ((lastChar > 0x19) & (lastChar < 0xF0)) {
            
            uint8_t* textLine = textLineA;
            if (cursorLine == 0) textLine = textLineA;
            if (cursorLine == 1) textLine = textLineB;
            if (cursorLine == 2) textLine = textLineC;
            if (cursorLine == 3) textLine = textLineD;
            if (cursorLine == 4) textLine = textLineE;
            if (cursorLine == 5) textLine = textLineF;
            if (cursorLine == 6) textLine = textLineG;
            if (cursorLine == 7) textLine = textLineH;
            
            // Shift down the EOL character
            
            // Find EOL
            uint8_t posEOL = 0;
            
            for (uint8_t i=0; i < 21; i++) {
                posEOL = i;
                if (textLine[i] == '\n') 
                    break;
            }
            
            if (posEOL < 19) {
                
                for (uint8_t i=posEOL; i >= 0; i--) {
                    
                    textLine[cursorPos + i + 1] = textLine[cursorPos + i];
                    
                    if (i == 0) 
                        break;
                }
                
            }
            
            // Apply the new character
            if (cursorPos < 19) {
                
                textLine[cursorPos] = lastChar;
                
                if (posEOL < 19) 
                    cursorPos++;
                
            }
            
            doUpdateFrame = 1;
        }
        
        
        //
        // Escape exit menu
        // Save/Quit/Cancel menu option
        
        if (lastChar == 0x07) {
            
            ConsoleClearScreen();
            
            // Filename
            ConsoleSetCursor(0, 0);
            print(filename, filename_length + 1);
            
            printSpace(2);
            
            // File size
            */
            
            
            /*
            if (fileSize > 99) {
                printSpace(13);
            } else {
                printSpace(14);
            }
            */
            
            
            
            
            
            /*
            printInt( fileSize );
            printChar('b');
            
            // Divider line
            ConsoleSetCursor(1, 0);
            for (uint8_t i=0; i < 20; i++) 
                printChar('=');
            
            ConsoleSetCursor(2, 0);
            print(msgSaveQuit, sizeof(msgSaveQuit));
            
            
            
            ConsoleSetBlinkRate(0);
            
            uint8_t answere = 0;
            while (answere == 0) {
                
                currentChar = ConsoleGetRawChar();
                
                if (lastChar == currentChar) 
                    continue;
                
                lastChar = currentChar;
                
                // Save
                if (lastChar == 's') {
                    
                    uint8_t saveBuffer[fileSize];
                    
                    for (uint16_t i=0; i < fileSize; i++) 
                        saveBuffer[i] = textBuffer[i];
                    
                    uint8_t saveIndex = 0;
                    
                    for (uint8_t i=0; i < 8; i++) {
                        
                        uint8_t* textSource = textLineA;
                        
                        if (i == 0) textSource = textLineA;
                        if (i == 1) textSource = textLineB;
                        if (i == 2) textSource = textLineC;
                        if (i == 3) textSource = textLineD;
                        if (i == 4) textSource = textLineE;
                        if (i == 5) textSource = textLineF;
                        if (i == 6) textSource = textLineG;
                        if (i == 7) textSource = textLineH;
                        
                        for (uint8_t a=0; a <= 21; a++) {
                            saveBuffer[saveIndex] = textSource[a];
                            saveIndex++;
                            if (textSource[a] == '\n') break;
                        }
                    }
                    
                    // Save file data
                    fsFileWrite(saveBuffer, fileSize);
                    
                    ConsoleSetCursor(3, 15);
                    print(msgSaved, sizeof(msgSaved));
                    
                }
                
                // Quit
                if (lastChar == 'q') {
                    
                    ConsoleClearScreen();
                    ConsoleSetCursor(0, 0);
                    
                    lastChar = ConsoleGetLastChar();
                    
                    ConsoleSetBlinkRate(CURSOR_BLINK_RATE);
                    
                    fsFileClose();
                    
                    return;
                }
                
                // Escape
                if (lastChar == 0x07) 
                    answere = 2;
                
                continue;
            }
            
            // Quit the application
            if (answere == 2) {
                
                fsFileClose();
                
                ConsoleClearScreen();
            }
            
            doUpdateFrame = 1;
            
            ConsoleSetBlinkRate(CURSOR_BLINK_RATE);
        }
        
        
        continue;
    }
    
    */
    
    return;
}

void registerCommandEDIT(void) {
    
    uint8_t editCommandName[] = "edit";
    
    ConsoleRegisterCommand(editCommandName, sizeof(editCommandName), functionEDIT);
    
    return;
}

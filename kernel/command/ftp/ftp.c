#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/network/network.h>

#include <kernel/command/ftp/ftp.h>

void functionFTP(uint8_t* param, uint8_t param_length) {
    
    //
    // Transfer the file
    
    if ((param[0] == 's') & (param[1] == 'e') & (param[2] == 'n') & (param[3] == 'd')) {
        
        uint8_t buffer[] = "String of text";
        uint8_t bufferSize = sizeof(buffer);
        
        ntSend(buffer, bufferSize);
        
        return;
    }
    
    
    //
    // Transfer the file
    
    if ((param[0] == 'r') & (param[1] == 'e') & (param[2] == 'c') & (param[3] == 'v')) {
        
        ConsoleCursorDisable();
        
        uint8_t numberOfBytes = 0;
        
        uint8_t buffer[80];
        
        uint16_t displayUpdate = 0;
        
        ConsoleClearScreen(' ');
        
        while (1) {
            
            numberOfBytes = ntReceive(buffer, 80);
            
            displayUpdate++;
            if (displayUpdate > 300) {
                
                ConsoleSetCursor(0, 0);
                
                print(buffer, numberOfBytes);
                
                displayUpdate = 0;
            }
            
        }
        
        return;
    }
    
    return;
}


void registerCommandFTP(void) {
    
    uint8_t ftpCommandName[] = "ftp";
    
    ConsoleRegisterCommand(ftpCommandName, sizeof(ftpCommandName), functionFTP);
    
    return;
}

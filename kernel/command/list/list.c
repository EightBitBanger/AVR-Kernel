#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/list/list.h>

uint8_t page = 0;
uint8_t mode = 0;

void functionList(uint8_t* param, uint8_t param_length) {
    
    if (is_number(&param[0]) == 1) 
        page = param[0] - '0';
    
    if (is_letter(&param[0]) == 1) {
        
        // Data dump mode
        if (param[0] == 'd') 
            mode = 0;
        
        // Hex dump mode
        if (param[0] == 'h') 
            mode = 1;
        
    }
    
    
    uint8_t buffer[200];
    
    uint8_t pageSize = 20 * 8;
    
    for (uint8_t i=0; i < pageSize; i++) 
        fs_read_byte((page * pageSize) + i, &buffer[i]);
    
    page++;
    
    // Print as HEX
    
    if (mode == 1) {
        
        for (uint8_t i=0; i < 30; i++) {
            
            uint8_t hex[2];
            
            int_to_hex_string( buffer[i], &hex[0] );
            
            print(hex, sizeof(hex) + 1);
            
            if ((i == 9) | (i == 19))
                printLn();
            
        }
        
        printLn();
        
    } else {
        
        // Print as text
        
        for (uint8_t i=0; i < 7; i++) {
            
            print( &buffer[20 * i], 21 );
            
            printLn();
            
        }
        
    }
    
    return;
}



void registerCommandList(void) {
    
    uint8_t listCommandName[] = "list";
    
    ConsoleRegisterCommand(listCommandName, sizeof(listCommandName), functionList);
    
    return;
}

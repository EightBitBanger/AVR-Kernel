#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/command/cap/cap.h>

void functionCAP(uint8_t* param, uint8_t param_length) {
    
    uint8_t msgBytesTotal[]       = "bytes";
    uint8_t msgDeviceNotReady[]   = "Device not ready";
    
    uint32_t deviceCapacity = fsDeviceGetCapacity();
    
    // Less than one K check
    if (deviceCapacity < 1024) {
        
        print(msgDeviceNotReady, sizeof(msgDeviceNotReady));
        printLn();
        
        return;
    }
    
    uint32_t numberOfKBytes = deviceCapacity / 1024;
    
    uint8_t integer[10];
    
    uint8_t place = int_to_string( numberOfKBytes, integer);
    
    print( &integer[0], place + 1 );
    
    printChar('k');
    printSpace(1);
    
    print( &msgBytesTotal[0], sizeof(msgBytesTotal) );
    
    printLn();
    
    return;
}

void registerCommandCAP(void) {
    
    uint8_t capCommandName[] = "cap";
    
    ConsoleRegisterCommand(capCommandName, sizeof(capCommandName), functionCAP);
    
    return;
}

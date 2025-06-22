#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/command/repair/repair.h>

void functionRepair(uint8_t* param, uint8_t param_length) {
    
    /*
    
    uint8_t buffer[fs_sector_size];
    
    // Get header sector
    uint32_t currentDevice = fsGetDevice();
    
    for (uint8_t i=0; i < fs_sector_size; i++) 
        fs_read_byte(currentDevice + i, &buffer[i]);
    
    // Check header byte
    if (buffer[0] != 0x13) {
        
        uint8_t headerByte = 0x13;
        
        // Attempt to fix the header byte
        fs_write_byte(currentDevice, headerByte);
        
    }
    
    // Check hardware name
    if ((buffer[1] != 'f') | (buffer[2] != 's')) {
        
        uint8_t deviceName[2] = {'f', 's'};
        
        // Attempt to fix the header byte
        fs_write_byte(currentDevice + 1, deviceName[0]);
        fs_write_byte(currentDevice + 2, deviceName[1]);
        
    }
    
    //union Pointer sizePointer;
    
    // Get device capacity
    //for (uint8_t i=0; i < 4; i++) 
    //    sizePointer.byte_t[i] = buffer[i + DEVICE_OFFSET_CAPACITY];
    
    */
    
    return;
}

void registerCommandRepair(void) {
    
    uint8_t repairCommandName[] = "fsck";
    
    ConsoleRegisterCommand(repairCommandName, sizeof(repairCommandName), functionRepair);
    
    return;
}

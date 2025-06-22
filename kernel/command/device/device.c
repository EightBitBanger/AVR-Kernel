#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/device/device.h>

void functionDevice(uint8_t* param, uint8_t param_length) {
    
    
    uint8_t lineCount = 0;
    
    if ((param[0] == 'l') & 
        (param[1] == 's')) {
        
        //
        // List device detected on the bus
        if ((param[0] == 'l') & 
            (param[1] == 's')) {
            
            uint8_t numberOfDevices = GetNumberOfDevices();
            
            for (uint8_t i=0; i < numberOfDevices; i++) {
                
                struct Device* devicePtr = GetHardwareDeviceByIndex(i);
                
                if (is_letter(&devicePtr->device_name[0]) == 0) 
                    continue;
                
                for (uint8_t a=0; a < DEVICE_NAME_LENGTH; a++) 
                    printChar(devicePtr->device_name[a]);
                
                printLn();
                
                lineCount++;
                if (lineCount > 6) {
                    
                    if (printPause() == 1) 
                        break;
                    
                    lineCount = 0;
                    
                    continue;
                }
                
            }
            
            lineCount = 0;
        }
        
        
        
        return;
    }
    
    
    //
    // Display linked drivers
    
    if ((param[0] == 'l') & 
        (param[1] == 's')) {
        
        uint8_t numberOfDrivers = GetNumberOfDrivers();
        
        for (uint8_t i=0; i < numberOfDrivers; i++) {
            
            struct Driver* drv = GetDriverByIndex( i );
            
            if ((param[3] == 'l') & 
                (param[4] == 'n') & 
                (param[5] == 'k')) {
                
                if (drv->is_linked == 0) 
                    continue;
            }
            
            for (uint8_t i=0; i < DEVICE_NAME_LENGTH; i++) {
                if (drv->device.device_name[i] != 0) {
                    printChar(drv->device.device_name[i]);
                } else {
                    printSpace(1);
                }
            }
            
            uint8_t addr = ((drv->device.hardware_address - PERIPHERAL_ADDRESS_BEGIN) + 0x10000) / 0x10000;
            
            if (drv->device.hardware_address != 0) {
                
                uint8_t deviceAddr[] = "0x00000";
                
                deviceAddr[2] += addr;
                print(&deviceAddr[0], sizeof(deviceAddr));
                
            }
            
            printLn();
            
            if (lineCount > 6) {
                
                printPause();
                
                continue;
            }
            
            lineCount++;
            
        }
        
        lineCount = 0;
        return;
    }
    
    //
    // Usage
    
    uint8_t msgHelpA[] = "-drv";
    uint8_t msgHelpB[] = "-drv lnk";
    uint8_t msgHelpC[] = "-dev";
    print(msgHelpA, sizeof(msgHelpA));
    printLn();
    print(msgHelpB, sizeof(msgHelpB));
    printLn();
    print(msgHelpC, sizeof(msgHelpC));
    printLn();
    
    return;
}



void registerCommandDevice(void) {
    
    uint8_t deviceCommandName[] = "device";
    
    ConsoleRegisterCommand(deviceCommandName, sizeof(deviceCommandName), functionDevice);
    
    return;
}

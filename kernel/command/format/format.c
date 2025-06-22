#include <avr/io.h>

#include <kernel/delay.h>
#include <kernel/kernel.h>

#include <kernel/command/format/format.h>

void functionFORMAT(uint8_t* param, uint8_t param_length) {
    
    uint32_t deviceCapacity = 0;
    
    uint8_t quickly = 0;
    
    // Process parameters
    
    for (uint8_t p=0; p < 10; p++) {
        
        uint8_t* paramBegin = ConsoleGetParameter(p, '-');
        if (paramBegin == 0) 
            break;
        
        if (paramBegin[0] == '8') {deviceCapacity = FORMAT_CAPACITY_8K; continue;}
        if (paramBegin[0] == '1' && paramBegin[1] == '6') {deviceCapacity = FORMAT_CAPACITY_16K; continue;}
        if (paramBegin[0] == '3' && paramBegin[1] == '2') {deviceCapacity = FORMAT_CAPACITY_32K; continue;}
        
        if (paramBegin[0] == 'q') {quickly = 1;}
        
        
    }
    
    
    uint32_t deviceCapacityCurrent=0;
    if (deviceCapacity == 0) {
        deviceCapacityCurrent = FORMAT_CAPACITY_8K / 1024;
    } else {
        deviceCapacityCurrent = deviceCapacity / 1024;
    }
    
    // Quick format
    if (quickly == 0) {
        uint8_t deviceCapacityMsg[] = "k bytes";
        uint8_t deviceCapacityAmount[10];
        
        uint8_t place = int_to_string(deviceCapacityCurrent, &deviceCapacityAmount[0]);
        
        print(deviceCapacityAmount, place + 1);
        ConsoleSetCursorPosition( place );
        print(deviceCapacityMsg, sizeof(deviceCapacityMsg));
    } else {
        
        uint8_t deviceQuickFormat[] = "Quick format";
        print(deviceQuickFormat, sizeof(deviceQuickFormat));
    }
    
    printLn();
    
    uint8_t devicePressToContinue[] = "Press Y to begin";
    print(devicePressToContinue, sizeof(devicePressToContinue));
    printLn();
    
    if (ConsoleWait() != 'y') 
        return;
    
    uint32_t deviceCapacityBytes = (deviceCapacityCurrent * 1024);
    uint32_t cyclesPerPercent = (deviceCapacityCurrent * 1024) / 100;
    
    uint32_t sector = 0;
    uint32_t sector_size = fs_sector_size;
    
    ConsoleCursorDisable();
    
    uint16_t percentage = 0;
    uint8_t percentageString[10] = {'0'};
    uint8_t percentageSymbole[1] = {'%'};
    uint8_t sectorCounter = 0;
    
    for (uint16_t i=0; i <= 1000; i++) {
        
        ConsoleSetCursorPosition(0);
        
        uint8_t place = int_to_string(percentage, &percentageString[0]);
        
        print(percentageString, place + 1);
        ConsoleSetCursorPosition(place);
        print(percentageSymbole, 2);
        
        percentage += 1;
        
        if (percentage > 99) 
            percentage = 99;
        
        for (uint32_t c=0; c < cyclesPerPercent; c++) {
            
            sector++;
            
            if (sectorCounter < (sector_size - 1)) {
                
                // Check quick format
                if (quickly == 0) 
                    fs_write_byte(sector, ' ');
                
                sectorCounter++;
                
            } else {
                fs_write_byte(sector, FORMAT_SECTOR_EMPTY);
                sectorCounter = 0;
            }
            
            if (sector < deviceCapacityBytes) 
                continue;
            
            // Finish the format
            i = 1000;
            
            break;
        }
        
        continue;
    }
    
    _delay_ms(10);
    
    // Construct the root directory
    fsDeviceConstructAllocationTable(0, deviceCapacityBytes, sector_size);
    
    // Finish as 100%
    ConsoleSetCursorPosition(0);
    
    uint8_t oneHundredPercentMsg[] = "100%";
    print( oneHundredPercentMsg, sizeof(oneHundredPercentMsg) );
    
    fsWorkingDirectoryClear();
    
    ConsoleCursorEnable();
    
    printLn();
    
    return;
}

void registerCommandFormat(void) {
    
    uint8_t formatCommandName[] = "format";
    
    ConsoleRegisterCommand(formatCommandName, sizeof(formatCommandName), functionFORMAT);
    
    return;
}

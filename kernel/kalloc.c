#include <kernel/kalloc.h>

uint32_t totalAllocatedMemory = 0;

uint32_t kAllocGetTotal(void) {
    return totalAllocatedMemory;
}

void AllocateExternalMemory(void) {
    ConsoleCursorDisable();
    
    uint8_t totalString[10];
    memset(totalString, 0x20, 10);
    
    struct Bus memoryBus;
	memoryBus.read_waitstate  = 20;
	memoryBus.write_waitstate = 20;
	
    uint32_t address=0;
    uint8_t buffer=0;
    uint8_t place;
    uint16_t counter=0;
    
    for (address=0x00000000; address < MAX_MEMORY_SIZE; address++) {
        
        mmio_write_byte(&memoryBus, address, 0xff);
        mmio_read_byte(&memoryBus, address, &buffer);
        if (buffer != 0xff) 
            break;
        
        // Print total allocated memory
        
        counter++;
        if (counter < 1024) 
            continue;
        
        counter = 0;
        
        place = int_to_string(address + 1, totalString) + 1;
        
        ConsoleSetCursor(0, 0);
        print(totalString, place);
    }
    
    totalAllocatedMemory = address + 1;
    
    printSpace(1);
    
    uint8_t byteFreeString[] = "bytes free";
    print(byteFreeString, sizeof(byteFreeString));
    
    printLn();
}



#include <kernel/virtual/virtual.h>

// Memory segmentation base offsets

#define __KERNEL_VIRTUAL_ACCESS_BEGIN__    0x00000000
#define __KERNEL_VIRTUAL_ACCESS_END__      0xffffffff

#define __USER_VIRTUAL_ACCESS_BEGIN__      0x00004000
#define __USER_VIRTUAL_ACCESS_END__        0xffffffff

uint32_t __virtual_address_begin__ = __KERNEL_VIRTUAL_ACCESS_BEGIN__;
uint32_t __virtual_address_end__   = __KERNEL_VIRTUAL_ACCESS_END__;

uint32_t __heap_begin__  = 0x00000000;
uint32_t __heap_end__    = 0xffffffff;


int8_t VirtualAccessGetMode(void) {
    
    if (__virtual_address_begin__ == __KERNEL_VIRTUAL_ACCESS_BEGIN__) 
        return VIRTUAL_ACCESS_MODE_KERNEL;
    if (__virtual_address_begin__ == __USER_VIRTUAL_ACCESS_BEGIN__) 
        return VIRTUAL_ACCESS_MODE_USER;
    
    return -1;
}

void VirtualAccessSetMode(uint8_t mode) {
    
    switch (mode) {
        default:
        case VIRTUAL_ACCESS_MODE_KERNEL: 
            __virtual_address_begin__ = __KERNEL_VIRTUAL_ACCESS_BEGIN__;
            __virtual_address_end__   = __KERNEL_VIRTUAL_ACCESS_END__;
            break;
        
        case VIRTUAL_ACCESS_MODE_USER: 
            __virtual_address_begin__ = __USER_VIRTUAL_ACCESS_BEGIN__;
            __virtual_address_end__   = __USER_VIRTUAL_ACCESS_END__;
            break;
    }
    
    return;
}


void VirtualWrite(uint32_t address, uint8_t* byte, uint32_t size) {
    /*
    VirtualBegin();
    
    uint32_t sector = 0;
    uint32_t sectorCounter = 0;
    
    for (uint32_t i=0; i < size; i++) {
        
        // Skip the sector marker byte
        if (sectorCounter == (fs_sector_size - 1)) {
            sectorCounter=0;
            sector++;
        }
        
        uint32_t offset = address + fs_sector_size + sector + 1;
        
        // Check kernel memory segmentation fault
        if (offset < __virtual_address_begin__ || 
            offset > __virtual_address_end__) {
            
            kThrow(HALT_SEGMENTATION_FAULT, offset);
        }
        
        // Check process memory segmentation
        if (offset < __heap_begin__ || 
            offset >= __heap_end__) {
            
            kThrow(HALT_SEGMENTATION_FAULT, offset);
        }
        
        fs_write_byte(offset, byte[i]);
        
        sectorCounter++;
        sector++;
        
        continue;
    }
    
    VirtualEnd();
    */
    return;
}


void VirtualRead(uint32_t address, uint8_t* byte, uint32_t size) {
    /*
    VirtualBegin();
    
    uint32_t sector = 0;
    uint32_t sectorCounter = 0;
    
    for (uint32_t i=0; i < size; i++) {
        
        // Skip the sector marker byte
        if (sectorCounter == (fs_sector_size - 1)) {
            sectorCounter=0;
            sector++;
        }
        
        uint32_t offset = address + fs_sector_size + sector + 1;
        
        // Check segmentation fault
        if (offset < __virtual_address_begin__ || 
            offset > __virtual_address_end__) {
            
            kThrow(HALT_SEGMENTATION_FAULT, offset);
        }
        
        // Check process memory segmentation
        if (offset < __heap_begin__ || 
            offset >= __heap_end__) {
            
            kThrow(HALT_SEGMENTATION_FAULT, offset);
        }
        
        fs_read_byte(offset, &byte[i]);
        
        sectorCounter++;
        sector++;
        
        continue;
    }
    
    VirtualEnd();
    */
    return;
}


uint8_t currentDevice = ' ';

void VirtualBegin(void) {
    /*
    if (currentDevice != ' ') {
        kThrow(HALT_SEGMENTATION_FAULT, 0);
        return;
    }
    currentDevice = fsDeviceGetRootLetter();
    fsDeviceSetRootLetter('x');
    */
    return;
}

void VirtualEnd(void) {
    /*
    if (currentDevice == ' ') {
        kThrow(HALT_SEGMENTATION_FAULT, 0);
        return;
    }
    fsDeviceSetRootLetter(currentDevice);
    currentDevice = ' ';
    */
    return;
}


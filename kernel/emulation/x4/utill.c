#include <stdint.h>
#include <kernel/emulation/x4/x4.h>


uint32_t AssembleJoin(uint8_t* buffer, uint32_t begin_address, uint8_t* source, uint32_t length) {
    
    for (uint32_t i=0; i < length; i++) 
        buffer[begin_address + i] = source[i];
    
    return begin_address + length;
}


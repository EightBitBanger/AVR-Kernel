#include <kernel/fs/fs.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

uint8_t fs_readb(uint32_t address) {
    return *(uintptr_t*)address;
}

void fs_writeb(uint32_t address, uint8_t byte) {
    *(uintptr_t*)address = byte;
}

void fs_mem_read(uint32_t address, void* destination, uint32_t size) {
    
}

void fs_mem_write(uint32_t address, const void* source, uint32_t size) {
    
}

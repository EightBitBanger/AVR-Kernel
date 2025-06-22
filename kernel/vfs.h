#ifndef __SYSCALL_VIRTUAL_FILE_SYSTEM_
#define __SYSCALL_VIRTUAL_FILE_SYSTEM_

#include <avr/io.h>
#include <stdio.h>
#include <string.h>

#include <kernel/kernel.h>

int32_t vfsOpen(uint8_t* filename, uint8_t nameLength);
uint8_t vfsClose(int32_t index);
uint8_t vfsRead(int32_t index, uint8_t buffer, uint32_t size);
uint8_t vfsWrite(int32_t index, uint8_t buffer, uint32_t size);
uint8_t vfsChdir(uint8_t* directoryName, uint8_t nameLength);
uint8_t vfsMkdir(uint8_t* filename, uint8_t nameLength);
uint8_t vfsRmdir(uint8_t* filename, uint8_t nameLength);
uint8_t vfsLookup(uint8_t* path, uint8_t pathLength);


struct VirtualFileSystemInterface {
    
    int32_t(*open)(uint8_t*, uint8_t);              // Open or create a file
    uint8_t(*close)(int32_t);                       // Close a file
    
    uint8_t(*read)(int32_t, uint8_t, uint32_t);     // Read data from a file
    uint8_t(*write)(int32_t, uint8_t, uint32_t);    // Write data to a file
    
    uint8_t(*chdir)(uint8_t*, uint8_t);             // Change directory
    uint8_t(*mkdir)(uint8_t*, uint8_t);             // Create directory
    uint8_t(*rmdir)(uint8_t*, uint8_t);             // Remove directory
    
};


#endif

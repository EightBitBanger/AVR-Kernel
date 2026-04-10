#ifndef _KERNEL_DIRECTORY_NODE_H_
#define _KERNEL_DIRECTORY_NODE_H_

#include <stdint.h>

struct KernelDirectory {
    char name[16];
    
    uint32_t parent;
    
    uint32_t next;
    uint32_t prev;
    
    uint16_t size;
    uint32_t reference[32];
};

struct KernelDirectoryExtent {
    uint32_t next;
    uint32_t prev;
    
    uint16_t size;
    uint32_t reference[32];
};

#endif

#ifndef _PROCESS_BLOCK_H_
#define _PROCESS_BLOCK_H_

#include <kernel/bus/bus.h>

#include <stdint.h>

struct ProcessBlock {
    char name[16];
    
    uint64_t pid;
    uint16_t flags;
    uint8_t priority;
    
    uint32_t code_segment_address;
    uint32_t stack_segment_address;
};

#endif

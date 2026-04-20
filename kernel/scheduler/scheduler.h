#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <kernel/emulation/x4/x4.h>

struct __attribute__((packed)) ProcessBlock {
    char name[16];
    
    uint32_t pid;
    uint8_t flags;
    
    uint32_t code_segment_address;
    uint32_t stack_segment_address;
    
    struct X4Thread thread_main;
    
    uint8_t padding[3];
};

struct ProcessDescriptor {};


void scheduler_init(void);


uint32_t create_task(const char* name, uint32_t entry_point_address, uint16_t priority, uint8_t flags);

void scheduler_handler(void);



uint8_t execute_program(char* filename, char** args, uint8_t arg_count);

#endif

#ifndef SYSCALL_BOOT_H
#define SYSCALL_BOOT_H

#include <stdint.h>
#include <string.h>
#include <kernel/kernel.h>

#include <kernel/emulation/x4/x4.h>

uint8_t program[] = {
    0x89, 0x01, 0x00, 0xCD, 0x16, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x83, 0x06, 0x1B, 0x00, 0x00, 0x00, 0x9A, 0x15, 0x00, 0x00, 0x00, 0x89, 0x01, 0x09, 
    0xCD, 0x10, 0xCB, 0x41, 0x0D, 0x00, 
};

int call_routine_boot(int arg_count, char** args) {
    for (uint8_t i=0; i < 16; i++) {
        uint32_t code_segment = kmalloc(sizeof(program));
        kmemcpy(code_segment, program, sizeof(program));
        
        task_create(code_segment, 0, 0x00);
    }
    
}

#endif

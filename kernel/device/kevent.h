#ifndef _KERNEL_EVENT_H_
#define _KERNEL_EVENT_H_

#include <stdint.h>

struct KEvent {
    char name[16];
    
    uint8_t flags;
    uint8_t reserved;
    
    void(*callback)();
    
    uint8_t padding[4];
};

#endif

#ifndef _KERNEL_EVENT_H_
#define _KERNEL_EVENT_H_

#include <stdint.h>

struct KEvent {
    char name[32];
    char args[32];
    
    uint16_t flags;
};

#endif

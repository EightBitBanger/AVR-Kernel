#ifndef __VIRTUAL_MEMORY_ACCESS_
#define __VIRTUAL_MEMORY_ACCESS_

#define VIRTUAL_ACCESS_MODE_KERNEL    0
#define VIRTUAL_ACCESS_MODE_USER      1

#include <kernel/kernel.h>

extern uint32_t __virtual_address_begin__;
extern uint32_t __virtual_address_end__;

int8_t VirtualAccessGetMode(void);
void VirtualAccessSetMode(uint8_t mode);

void VirtualWrite(uint32_t address, uint8_t* byte, uint32_t size);
void VirtualRead(uint32_t address, uint8_t* byte, uint32_t size);

void VirtualBegin(void);
void VirtualEnd(void);

#endif

#ifndef KERNEL_EVENT_SYSTEM_H
#define KERNEL_EVENT_SYSTEM_H

#include <kernel/events.h>

#define KEVENT_EXECUTE     0x0001

bool kernel_event_send(uint8_t flags, const char* name, const char* arguments);
bool kernel_event_remove(const char* name);

void kernel_event_update(void);

#endif

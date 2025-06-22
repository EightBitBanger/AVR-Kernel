#ifndef __SYSCALL_SPEAKER_BEEP_
#define __SYSCALL_SPEAKER_BEEP_

#include <kernel/kernel.h>

void beep(uint16_t duration, uint16_t frequency);

void sysbeep(void);

#endif

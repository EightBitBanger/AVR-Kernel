#include <kernel/syscalls/timer/timer.h>

extern volatile uint64_t system_timer_ms;

uint64_t time_ms(void) {
    
    return system_timer_ms;
}

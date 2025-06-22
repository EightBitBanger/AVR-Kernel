#include <avr/io.h>
#include <stddef.h>

#include <kernel/mutex.h>

uint8_t MutexLock(struct mutex* mux) {
    if (mux->lock == 0) {// Lock it
        mux->lock = 1;
        return 0;
    }
    return 1;
}

void MutexUnlock(struct mutex* mux) {
    mux->lock = 0;
    return;
}

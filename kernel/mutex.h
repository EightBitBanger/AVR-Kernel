#ifndef KERNEL_MUTEX_H
#define KERNEL_MUTEX_H

#include <kernel/kernel.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

struct Mutex {
    uint8_t lock;
};

inline void mutex_initiate(struct Mutex* mux) {
    mux->lock = 0;
}

inline void mutex_lock(struct Mutex* mux) {
    mux->lock = 1;
}

inline void mutex_unlock(struct Mutex* mux) {
    mux->lock = 0;
}

inline bool mutex_is_locked(struct Mutex* mux) {
    if (mux->lock == 0) 
        return false;
    return true;
}

#endif

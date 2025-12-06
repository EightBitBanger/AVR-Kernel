#ifndef __KERNEL_MUTEX_LOCK_
#define __KERNEL_MUTEX_LOCK_

struct Mutex {
    uint8_t lock;
};

uint8_t MutexLock(struct Mutex* mux);
void MutexUnlock(struct Mutex* mux);

#endif

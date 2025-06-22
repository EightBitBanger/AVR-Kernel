#ifndef __KERNEL_MUTEX_LOCK_
#define __KERNEL_MUTEX_LOCK_

struct mutex {
    
    uint8_t lock;
    
};

uint8_t MutexLock(struct mutex* mux);
void MutexUnlock(struct mutex* mux);

#endif

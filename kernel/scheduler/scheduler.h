#ifndef _KERNEL_SCHEDULER_H_
#define _KERNEL_SCHEDULER_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD
} ThreadState;

typedef enum {
    PRIORITY_IDLE       = 1,  // 1 tick   (5ms)
    PRIORITY_LOW        = 2,  // 2 ticks  (10ms)
    PRIORITY_NORMAL     = 3,  // 3 ticks  (15ms)
    PRIORITY_HIGH       = 4,  // 4 ticks  (20ms)
    PRIORITY_REALTIME   = 5   // 5 ticks  (25ms)
} ThreadPriority;

typedef struct ThreadBlockType {
    uint32_t esp;               // Current stack pointer
    uint32_t cr3;               // Page directory
    ThreadState state;          // Thread state
    void* stack_base;           // Base pointer for reaper
    
    ThreadPriority priority;    // Thread base priority
    uint32_t ticks_remaining;   // Ticks left in current quantum
    uint32_t wake_tick;         // System tick when thread should unblock
    
    struct ThreadBlockType* next;
} ThreadBlock;

void scheduler_init(void);

uint32_t thread_get_count(void);

ThreadBlock* thread_create(void (*entry_point)(void), ThreadPriority priority);

void thread_yield(void);
void thread_sleep(uint32_t ticks);

uint32_t thread_handler_c(uint32_t current_esp);

#endif

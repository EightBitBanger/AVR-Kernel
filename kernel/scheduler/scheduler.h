
#ifndef _KERNEL_SCHEDULER_H_
#define _KERNEL_SCHEDULER_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SLEEPING,            // Waiting for wake_tick (thread_sleep)
    THREAD_BLOCKED,             // Waiting for an explicit wake (mutex, events)
    THREAD_DEAD
} ThreadState;

// Quantum length always comes from the BASE priority; the effective
// (dynamic) priority only decides who runs next.
typedef enum {
    PRIORITY_IDLE       = 1,  // 1 tick   1ms
    PRIORITY_LOW        = 2,  // 2 ticks  2ms
    PRIORITY_NORMAL     = 3,  // 3 ticks  3ms
    PRIORITY_HIGH       = 4,  // 4 ticks  4ms
    PRIORITY_REALTIME   = 5   // 5 ticks  5ms
} ThreadPriority;

//
// Priority degradation / anti-starvation tuning (1 timer tick = 1 ms).
// All can be overridden with -D.
//
// - Degradation: every time a thread burns SCHED_DEGRADE_QUANTA full quanta
//   of CPU (counted across preemptions and yields, not reset by a short
//   block), its effective priority drops one level, down to PRIORITY_IDLE.
// - Recovery: blocking/sleeping wins one level back and halves the usage
//   counter, and so does every SCHED_RECOVER_MS spent off the CPU. Threads
//   never recover above their base priority.
// - Starvation boost: a thread that has been READY for SCHED_STARVATION_MS
//   without getting the CPU runs at PRIORITY_REALTIME for one quantum.
//   This bounds the wait of every runnable thread, whatever the load.

#ifndef SCHED_DEGRADE_QUANTA
#define SCHED_DEGRADE_QUANTA    2
#endif

#ifndef SCHED_RECOVER_MS
#define SCHED_RECOVER_MS        20
#endif

#ifndef SCHED_STARVATION_MS
#define SCHED_STARVATION_MS     100
#endif

// 1 = REALTIME threads degrade like everyone else. 0 = they keep their level
// (other threads still reach them through the starvation boost).
#ifndef SCHED_DEGRADE_REALTIME
#define SCHED_DEGRADE_REALTIME  0
#endif

typedef struct ThreadBlockType {
    uint32_t esp;               // Current stack pointer
    uint32_t cr3;               // Page directory
    ThreadState state;          // Thread state
    void* stack_base;           // Base of the stack allocation (incl. guard page), NULL for the boot thread
    uint8_t* fpu_state;         // 512-byte, 16-byte aligned FXSAVE area

    ThreadPriority priority;    // Thread base priority (sets the quantum and the recovery ceiling)
    uint32_t ticks_remaining;   // Ticks left in current quantum
    uint64_t wake_tick;         // Millisecond timestamp when a SLEEPING thread becomes READY

    // Dynamic priority
    ThreadPriority dyn_priority;    // Effective priority after degradation / recovery
    bool boosted;                   // Starvation boost: runs at REALTIME for one quantum
    uint32_t cpu_used_ms;           // Recent CPU use, decays while off the CPU
    uint64_t recover_mark_ms;       // Last time it ran (or recovered a level)
    uint64_t wait_mark_ms;          // Last time it was not READY (start of the current wait)

    struct ThreadBlockType* next;       // Run queue link (circular)
    struct ThreadBlockType* reap_next;  // Reaper list link

    struct Event* wait_event;           // Event this thread is queued on, NULL if none
    struct ThreadBlockType* wait_next;  // Event wait list link
    bool wait_signaled;                 // Set by event_signal when it wakes this thread
} ThreadBlock;

// Auto-reset event.
//
// event_signal() wakes every thread currently waiting. If nobody is waiting,
// the event stays signaled and the next event_wait() returns immediately
// (consuming it), so a wakeup that races ahead of the waiter is not lost.
// event_signal() is safe to call from interrupt handlers.
typedef struct Event {
    volatile bool signaled;
    ThreadBlock* waiters;
} Event;

#define EVENT_INITIALIZER { false, NULL }

void event_init(Event* event);
void event_signal(Event* event);

// Block until the event is signaled or timeout_ms elapses (0 = no timeout).
// Returns true if woken by a signal, false on timeout.
bool event_wait(Event* event, uint32_t timeout_ms);

// Wakes the kernel event thread early (defined in boot.c)
extern Event kernel_wakeup_event;

void scheduler_init(void);

uint32_t thread_get_count(void);

ThreadBlock* thread_create(void (*entry_point)(void), ThreadPriority priority);

// Priority the scheduler is currently using for this thread
// (degraded, recovered or starvation-boosted). For task lists / debugging.
ThreadPriority thread_get_effective_priority(const ThreadBlock* thread);

void thread_yield(void);
void thread_sleep(uint32_t ms);

uint32_t thread_handler_c(uint32_t current_esp);

#endif

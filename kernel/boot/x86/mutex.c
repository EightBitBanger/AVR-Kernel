#include <kernel/mutex.h>
#include <kernel/memory/malloc.h>
#include <kernel/arch/x86/irq.h>

// Reference the current running thread defined in scheduler.c
extern ThreadBlock* current_thread;

void mutex_init(mutex_t* mutex) {
    if (!mutex) return;
    
    uint32_t flags = irq_save();
    mutex->locked = false;
    mutex->owner = NULL;
    mutex->wait_head = NULL;
    mutex->wait_tail = NULL;
    irq_restore(flags);
}

void mutex_lock(mutex_t* mutex) {
    if (!mutex) return;
    
    uint32_t flags = irq_save();
    
    // Fast path: lock is unowned
    if (!mutex->locked) {
        mutex->locked = true;
        mutex->owner = current_thread;
        irq_restore(flags);
        return;
    }
    
    // Slow path: lock is held. Queue current thread using local stack frame node.
    // (Safe because the thread stack remains allocated while blocked)
    MutexWaiter waiter;
    waiter.thread = current_thread;
    waiter.next = NULL;
    
    if (mutex->wait_tail != NULL) {
        mutex->wait_tail->next = &waiter;
    } else {
        mutex->wait_head = &waiter;
    }
    mutex->wait_tail = &waiter;
    
    // Mark current thread as BLOCKED and yield CPU.
    // THREAD_BLOCKED is only ever cleared by mutex_unlock (sleepers use
    // THREAD_SLEEPING), so when we resume, ownership has been handed to us.
    current_thread->state = THREAD_BLOCKED;
    
    // Yield with interrupts still disabled so nothing can run between
    // queueing ourselves and switching away. int $0x80 works with IF=0 and
    // execution resumes here (IF=0) once mutex_unlock readies us.
    thread_yield();
    
    irq_restore(flags);
}

bool mutex_trylock(mutex_t* mutex) {
    if (!mutex) return false;
    
    uint32_t flags = irq_save();
    
    if (!mutex->locked) {
        mutex->locked = true;
        mutex->owner = current_thread;
        irq_restore(flags);
        return true;
    }
    
    irq_restore(flags);
    return false;
}

void mutex_unlock(mutex_t* mutex) {
    if (!mutex) return;
    
    uint32_t flags = irq_save();
    
    // Ensure only the thread holding the lock can release it
    if (mutex->owner != current_thread) {
        irq_restore(flags);
        return;
    }
    
    // Check if threads are waiting in the queue
    if (mutex->wait_head != NULL) {
        // Dequeue next thread
        MutexWaiter* next_waiter = mutex->wait_head;
        mutex->wait_head = next_waiter->next;
        if (mutex->wait_head == NULL) {
            mutex->wait_tail = NULL;
        }
        
        // Direct handoff: pass ownership directly to dequeued thread
        mutex->owner = next_waiter->thread;
        next_waiter->thread->state = THREAD_READY;
    } else {
        // Queue is empty: unlock completely
        mutex->locked = false;
        mutex->owner = NULL;
    }
    
    irq_restore(flags);
}


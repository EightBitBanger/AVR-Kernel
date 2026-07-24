#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/virtual/vmm.h>
#include <kernel/memory/malloc.h>
#include <kernel/scheduler/scheduler.h>

#include <kernel/util/string.h>
#include <kernel/util/timer.h>

struct ThreadInterruptFrame {
    uint32_t gs, fs, es, ds;                               // Pushed by our ISR
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; // Pushed by pushal
    uint32_t eip, cs, eflags;                              // Pushed by CPU on interrupt
} __attribute__((packed));

extern uint32_t page_directory[];

ThreadBlock* current_thread = NULL;
ThreadBlock* thread_queue = NULL;

static ThreadBlock* thread_to_reap = NULL;
uint16_t five_millisecond_timer = 0;

static void thread_exit(void) {
    __asm__ volatile("cli"); // Disable interrupts while modifying scheduler state
    
    current_thread->state = THREAD_DEAD;
    
    __asm__ volatile("sti"); // Re-enable interrupts
    
    // Wait for the timer interrupt to schedule another thread.
    // This loop will never be returned to once scheduled out.
    while (1) {
        __asm__ volatile("hlt");
    }
}

static void unlink_thread(ThreadBlock* dead) {
    if (!thread_queue || !dead) return;
    
    // If it's the only thread left in the queue
    if (dead->next == dead) {
        thread_queue = NULL;
        return;
    }
    
    // Locate the previous node in the ring
    ThreadBlock* prev = dead;
    while (prev->next != dead) {
        prev = prev->next;
    }
    
    // Bypass the dead node
    prev->next = dead->next;
    
    // Shift list head if necessary
    if (thread_queue == dead) {
        thread_queue = dead->next;
    }
}

static void reap_thread(void) {
    if (thread_to_reap != NULL) {
        if (thread_to_reap->stack_base) {
            vmm_free_pages(thread_to_reap->stack_base, 1);
        }
        free(thread_to_reap);
        thread_to_reap = NULL;
    }
}

void thread_yield(void) {
    if (current_thread) 
        current_thread->ticks_remaining = 0;
    
    __asm__ volatile("int $0x80");
}

void thread_sleep(uint32_t ticks) {
    if (ticks == 0) {
        thread_yield();
        return;
    }
    
    __asm__ volatile("cli");
    current_thread->wake_tick = timer_get_ms() + ticks;
    current_thread->state = THREAD_BLOCKED;
    __asm__ volatile("sti");
    
    thread_yield(); // Give up the CPU right away
}

void scheduler_init(void) {
    ThreadBlock* main_thread = malloc(sizeof(ThreadBlock));
    if (!main_thread) return;
    
    main_thread->esp = 0;
    main_thread->cr3 = (uint32_t)page_directory;
    main_thread->state = THREAD_RUNNING;
    
    // Kernel thread gets high priority
    main_thread->priority = PRIORITY_NORMAL;
    main_thread->ticks_remaining = PRIORITY_NORMAL;
    
    thread_queue = main_thread;
    thread_queue->next = main_thread;
    current_thread = main_thread;
}

ThreadBlock* thread_create(void (*entry_point)(void), ThreadPriority priority) {
    ThreadBlock* thread = malloc(sizeof(ThreadBlock));
    if (!thread) return NULL;
    
    void* stack = vmm_alloc_pages(1);
    uint32_t stack_top = (uint32_t)stack + PAGE_SIZE;
    
    thread->stack_base = stack;
    
    stack_top -= sizeof(uint32_t);
    *(uint32_t*)stack_top = (uint32_t)thread_exit;
    
    stack_top -= sizeof(struct ThreadInterruptFrame);
    struct ThreadInterruptFrame* frame = (struct ThreadInterruptFrame*)stack_top;
    memset(frame, 0, sizeof(struct ThreadInterruptFrame));
    
    frame->eip = (uint32_t)entry_point;
    frame->cs = 0x08;
    frame->eflags = 0x202;
    frame->ds = 0x10;
    frame->es = 0x10;
    frame->fs = 0x10;
    frame->gs = 0x10;
    
    thread->esp = stack_top;
    thread->cr3 = (uint32_t)page_directory;
    thread->state = THREAD_READY;
    thread->priority = priority;
    thread->ticks_remaining = (uint32_t)priority;
    thread->next = NULL;
    
    if (!thread_queue) {
        thread_queue = thread;
        thread->next = thread;
    } else {
        ThreadBlock* temp = thread_queue;
        while (temp->next != thread_queue) {
            temp = temp->next;
        }
        temp->next = thread;
        thread->next = thread_queue;
    }
    
    return thread;
}

uint32_t thread_get_count(void) {
    if (!thread_queue) 
        return 0;
    
    uint32_t count = 0;
    ThreadBlock* head = thread_queue;
    
    // Count the head node
    if (head->state != THREAD_DEAD) 
        count++;
    
    // Advance to the second node and loop until we wrap back around to head
    ThreadBlock* current = head->next;
    while (current != head) {
        if (current->state != THREAD_DEAD) {
            count++;
        }
        current = current->next;
    }
    
    return count;
}

uint32_t thread_handler_c(uint32_t current_esp) {
    if (!thread_queue) 
        return current_esp;
    
    reap_thread();
    
    // Wake up sleeping threads
    ThreadBlock* temp = thread_queue;
    do {
        if (temp->state == THREAD_BLOCKED && timer_get_ms() >= temp->wake_tick) {
            temp->state = THREAD_READY;
        }
        temp = temp->next;
    } while (temp != thread_queue);
    
    // If current thread still has time slice remaining and is RUNNING, let it run
    if (current_thread->state == THREAD_RUNNING && current_thread->ticks_remaining > 1) {
        current_thread->ticks_remaining--;
        return current_esp;
    }
    
    current_thread->ticks_remaining = (uint32_t)current_thread->priority;
    
    // Save stack pointer for living threads
    if (current_thread->state != THREAD_DEAD) {
        current_thread->esp = current_esp;
        
        // Only demote to READY if it was actively RUNNING
        if (current_thread->state == THREAD_RUNNING) {
            current_thread->state = THREAD_READY;
        }
    }
    
    // Find the next READY/RUNNING thread
    ThreadBlock* next_thread = current_thread->next;
    while ((next_thread->state == THREAD_BLOCKED || next_thread->state == THREAD_DEAD) 
        && next_thread != current_thread) {
        
        if (next_thread->state == THREAD_DEAD) {
            ThreadBlock* dead = next_thread;
            next_thread = next_thread->next;
            unlink_thread(dead);
            thread_to_reap = dead;
        } else {
            next_thread = next_thread->next;
        }
    }
    
    current_thread = next_thread;
    current_thread->state = THREAD_RUNNING;
    
    return current_thread->esp;
}

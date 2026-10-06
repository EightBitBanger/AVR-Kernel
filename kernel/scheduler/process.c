#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <kernel/scheduler/process.h>
#include <kernel/scheduler/scheduler.h>

#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/irq.h>

#include <kernel/util/list.h>
#include <kernel/util/string.h>
#include <kernel/util/parser.h>

#define PAGE_SIZE           4096u
#define PAGE_MASK           (~(PAGE_SIZE - 1))
#define PD_ENTRIES          1024u
#define CR0_PG              0x80000000u
#define CR3_FLAGS_MASK      0x18u           // PWT | PCD

#define PID_SLOT_BITS       8
#define PID_SLOT_MASK       0xFFu
#define PID_GEN_MASK        0x007FFFFFu     // 23 bits: keeps the id positive

// Physical address of a kernel heap pointer. The kernel heap is assumed to
// be identity mapped; change this if the kernel moves to a higher half.
#define KERNEL_VIRT_TO_PHYS(p)  ((uint32_t)(uintptr_t)(p))
#define KERNEL_PHYS_TO_VIRT(a)  ((void*)(uintptr_t)(a))

typedef char process_max_fits_slot_field[(PROCESS_MAX <= 256) ? 1 : -1];

// One contiguous region of the process image
typedef struct {
    void* base;         // Page-aligned start, NULL if the segment is empty
    void* raw;          // Pointer returned by malloc (what gets freed)
    uint32_t size;      // Requested size in bytes
    bool owned;         // false: describes memory owned by someone else
} ProcessSegment;

typedef struct {
    ProcessState state;
    uint32_t generation;
    ProcessId pid;

    ThreadPriority priority;
    char name[PROCESS_NAME_MAX];
    char image_path[PROCESS_PATH_MAX];

    // Address space
    uint32_t* page_directory;   // Virtual address of the directory
    void* page_directory_raw;   // Allocation backing it
    uint32_t cr3;               // Physical address + flags, loaded per thread

    // Image
    ProcessSegment code;
    ProcessSegment data;
    ProcessSegment bss;
    ProcessSegment stack;       // Main thread's stack (owned by the scheduler)
    uint32_t entry_offset;      // Entry point, relative to code.base

    // Threads (list payloads are ThreadBlock*)
    struct list_node* threads_head;
    struct list_node* threads_tail;
    uint32_t thread_count;
} Process;

// Section sizes reported by the executable loader
typedef struct {
    uint32_t code_size;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t entry_offset;
} ProcessImageLayout;

static Process process_table[PROCESS_MAX];
static uint32_t process_count = 0;
static uint32_t kernel_cr3 = 0;

// CPU helpers

static inline uint32_t read_cr0(void) {
    uint32_t value;
    __asm__ volatile ("mov %%cr0, %0" : "=r" (value));
    return value;
}

static inline uint32_t read_cr3(void) {
    uint32_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r" (value));
    return value;
}

static inline void write_cr3(uint32_t value) {
    __asm__ volatile ("mov %0, %%cr3" : : "r" (value) : "memory");
}

// Handles

static ProcessId pid_encode(uint32_t generation, uint32_t slot) {
    return (ProcessId)(((generation & PID_GEN_MASK) << PID_SLOT_BITS) | (slot & PID_SLOT_MASK));
}

// Caller must hold the lock (interrupts disabled)
static Process* process_lookup(ProcessId pid) {
    if (pid <= 0)
        return NULL;
    uint32_t slot = (uint32_t)pid & PID_SLOT_MASK;
    if (slot >= PROCESS_MAX)
        return NULL;
    Process* p = &process_table[slot];
    if (p->state == PROCESS_STATE_FREE || p->pid != pid)
        return NULL;
    return p;
}

// Segments

static bool segment_alloc(ProcessSegment* seg, uint32_t size) {
    memset(seg, 0, sizeof(*seg));
    if (size == 0)
        return true;    // Empty segment: nothing to allocate
    if (size > 0xFFFFFFFFu - 2 * PAGE_SIZE)
        return false;

    // Whole pages, page aligned, so per-page permissions can be applied later
    uint32_t rounded = (size + PAGE_SIZE - 1) & PAGE_MASK;
    void* raw = malloc(rounded + PAGE_SIZE - 1);
    if (raw == NULL)
        return false;

    uintptr_t aligned = ((uintptr_t)raw + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
    memset((void*)aligned, 0, rounded);     // Also zero-fills .bss

    seg->raw = raw;
    seg->base = (void*)aligned;
    seg->size = size;
    seg->owned = true;
    return true;
}

static void segment_free(ProcessSegment* seg) {
    if (seg->owned && seg->raw != NULL)
        free(seg->raw);
    memset(seg, 0, sizeof(*seg));
}

// Address space

static bool address_space_create(Process* p) {
    ProcessSegment pd;
    if (!segment_alloc(&pd, PAGE_SIZE))
        return false;

    uint32_t* dir = (uint32_t*)pd.base;
    const uint32_t* kdir = (const uint32_t*)KERNEL_PHYS_TO_VIRT(kernel_cr3 & PAGE_MASK);

    // Kernel-space process: share every kernel page table
    memcpy(dir, kdir, PAGE_SIZE);

    // If the kernel uses a recursive mapping in the last slot, point this
    // copy at itself instead of at the kernel directory
    if ((kdir[PD_ENTRIES - 1] & PAGE_MASK) == (kernel_cr3 & PAGE_MASK)) {
        dir[PD_ENTRIES - 1] = (KERNEL_VIRT_TO_PHYS(dir) & PAGE_MASK) |
                              (kdir[PD_ENTRIES - 1] & ~PAGE_MASK);
    }

    p->page_directory = dir;
    p->page_directory_raw = pd.raw;
    p->cr3 = KERNEL_VIRT_TO_PHYS(dir) | (kernel_cr3 & CR3_FLAGS_MASK);
    return true;
}

static void address_space_destroy(Process* p) {
    // Only the directory is ours; the page tables belong to the kernel
    if (p->page_directory_raw != NULL)
        free(p->page_directory_raw);
    p->page_directory = NULL;
    p->page_directory_raw = NULL;
    p->cr3 = 0;
}

// Executable image (placeholders until the loader exists)

static bool process_image_probe(const char* path, ProcessImageLayout* layout) {
    // TODO: vfs_open(path), parse the executable header and fill in the
    // section sizes and entry point. Until then the image is empty.
    (void)path;
    memset(layout, 0, sizeof(*layout));
    return true;
}

static bool process_image_load(Process* p) {
    // TODO: copy the code and data sections from p->image_path into
    // p->code.base / p->data.base (.bss is already zeroed), then apply
    // page permissions (code read+execute, data/bss read+write).
    (void)p;
    return true;
}

// Main thread entry while no executable can be loaded. Once the loader is in
// place this becomes a trampoline that finds its process (e.g. by matching
// read_cr3() against process_table) and jumps to code.base + entry_offset.
static void process_entry_stub(void) {
    for (;;) {
        thread_sleep(1000);
    }
}

// Threads

static void thread_detach_from_event(ThreadBlock* t) {
    Event* event = t->wait_event;
    if (event == NULL)
        return;

    ThreadBlock** link = &event->waiters;
    while (*link != NULL) {
        if (*link == t) {
            *link = t->wait_next;
            break;
        }
        link = &(*link)->wait_next;
    }
    t->wait_event = NULL;
    t->wait_next = NULL;
}

// Caller must hold the lock
static void process_kill_threads(Process* p) {
    while (p->threads_head != NULL) {
        ThreadBlock* t = (ThreadBlock*)p->threads_head->data;

        thread_detach_from_event(t);    // Don't leave it in an event's wait list
        t->cr3 = kernel_cr3;            // Its own directory is about to be freed
        t->state = THREAD_DEAD;         // The scheduler's reaper frees the stack

        list_remove(&p->threads_head, &p->threads_tail, t);
    }
    p->thread_count = 0;
}

// Slot management

static void process_free_resources(Process* p) {
    segment_free(&p->code);
    segment_free(&p->data);
    segment_free(&p->bss);
    memset(&p->stack, 0, sizeof(p->stack));     // Not ours to free
    address_space_destroy(p);
}

// Caller must hold the lock
static void process_release_slot(Process* p) {
    uint32_t generation = (p->generation + 1) & PID_GEN_MASK;
    if (generation == 0)
        generation = 1;

    memset(p, 0, sizeof(*p));
    p->generation = generation;
    p->state = PROCESS_STATE_FREE;
    process_count--;
}

// Public API

ProcessId CreateProcess(const char* image_path, ThreadPriority priority) {
    if (image_path == NULL || image_path[0] == '\0')
        return PROCESS_ERR_INVALID_ARG;
    if (strnlen(image_path, PROCESS_PATH_MAX) >= PROCESS_PATH_MAX)
        return PROCESS_ERR_INVALID_ARG;
    if (priority < PRIORITY_IDLE || priority > PRIORITY_REALTIME)
        return PROCESS_ERR_INVALID_ARG;
    if ((read_cr0() & CR0_PG) == 0)
        return PROCESS_ERR_NO_PAGING;

    // Reserve a slot
    Process* p = NULL;
    uint32_t irq = irq_save();
    if (kernel_cr3 == 0)
        kernel_cr3 = read_cr3();    // First call happens from kernel context
    for (uint32_t slot = 0; slot < PROCESS_MAX; slot++) {
        if (process_table[slot].state == PROCESS_STATE_FREE) {
            p = &process_table[slot];
            if (p->generation == 0)
                p->generation = 1;
            p->state = PROCESS_STATE_CREATING;
            p->pid = pid_encode(p->generation, slot);
            process_count++;
            break;
        }
    }
    irq_restore(irq);
    if (p == NULL)
        return PROCESS_ERR_NO_SLOTS;

    // The slot is reserved (CREATING), so it can be filled in without the lock
    int error = PROCESS_OK;
    p->priority = priority;
    strncpy(p->image_path, image_path, PROCESS_PATH_MAX - 1);
    p->image_path[PROCESS_PATH_MAX - 1] = '\0';
    parse_get_filename(image_path, p->name, PROCESS_NAME_MAX);

    ProcessImageLayout layout;
    if (!process_image_probe(p->image_path, &layout)) {
        error = PROCESS_ERR_LOAD_FAILED;
        goto fail;
    }
    p->entry_offset = layout.entry_offset;

    if (!segment_alloc(&p->code, layout.code_size) ||
        !segment_alloc(&p->data, layout.data_size) ||
        !segment_alloc(&p->bss, layout.bss_size) ||
        !address_space_create(p)) {
        error = PROCESS_ERR_NO_MEMORY;
        goto fail;
    }

    if (!process_image_load(p)) {
        error = PROCESS_ERR_LOAD_FAILED;
        goto fail;
    }

    // Create the main thread with interrupts off so it cannot be scheduled
    // before it is switched to this process's address space
    irq = irq_save();
    ThreadBlock* thread = thread_create(process_entry_stub, priority);
    if (thread != NULL) {
        thread->cr3 = p->cr3;
        if (list_append(&p->threads_head, &p->threads_tail, thread)) {
            p->thread_count = 1;
            p->stack.base = thread->stack_base;
            p->stack.owned = false;
            p->state = PROCESS_STATE_RUNNING;
        } else {
            thread->cr3 = kernel_cr3;
            thread->state = THREAD_DEAD;
            thread = NULL;
        }
    }
    irq_restore(irq);

    if (thread == NULL) {
        error = PROCESS_ERR_THREAD;
        goto fail;
    }
    return p->pid;

fail:
    process_free_resources(p);
    irq = irq_save();
    process_release_slot(p);
    irq_restore(irq);
    return error;
}

int DestroyProcess(ProcessId pid) {
    uint32_t irq = irq_save();

    Process* p = process_lookup(pid);
    if (p == NULL || p->state != PROCESS_STATE_RUNNING) {
        irq_restore(irq);
        return PROCESS_ERR_NOT_FOUND;
    }
    p->state = PROCESS_STATE_DYING;

    // Destroying our own process: get off its page directory before freeing it
    bool self = (read_cr3() & PAGE_MASK) == (p->cr3 & PAGE_MASK);
    if (self)
        write_cr3(kernel_cr3);

    // Everything below runs with interrupts off. If we are one of the
    // threads being killed, a preemption here would never come back and the
    // process memory would leak.
    process_kill_threads(p);
    process_free_resources(p);
    process_release_slot(p);

    irq_restore(irq);

    if (self) {
        // Our ThreadBlock is now DEAD; wait for the scheduler to drop us
        for (;;) {
            thread_yield();
        }
    }
    return PROCESS_OK;
}

bool ProcessGetInfo(ProcessId pid, ProcessInfo* info) {
    if (info == NULL)
        return false;

    uint32_t irq = irq_save();
    Process* p = process_lookup(pid);
    if (p == NULL) {
        irq_restore(irq);
        return false;
    }

    info->pid = p->pid;
    info->state = p->state;
    info->priority = p->priority;
    memcpy(info->name, p->name, PROCESS_NAME_MAX);
    info->thread_count = p->thread_count;
    info->code_size = p->code.size;
    info->data_size = p->data.size;
    info->bss_size = p->bss.size;

    irq_restore(irq);
    return true;
}

uint32_t process_get_count(void) {
    return process_count;
}

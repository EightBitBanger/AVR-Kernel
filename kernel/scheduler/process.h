#ifndef _KERNEL_PROCESS_H_
#define _KERNEL_PROCESS_H_

#include <stdint.h>
#include <stdbool.h>

#include <kernel/scheduler/scheduler.h>

/*
 * Processes
 *
 * A process owns an address space (page directory), its image segments
 * (code, data, bss) and a list of threads.
 *
 * ProcessId layout: [ generation (23 bits) | slot (8 bits) ], always > 0.
 * The generation is bumped every time a slot is freed, so a stale id held
 * after DestroyProcess() can never refer to a newer process that happened
 * to reuse the same slot.
 *
 * For now everything runs in kernel space: each process gets its own page
 * directory, but it maps exactly what the kernel maps.
 */

#define PROCESS_MAX                64     // Must be <= 256 (8-bit slot field)
#define PROCESS_NAME_MAX           16
#define PROCESS_PATH_MAX          256

typedef int32_t ProcessId;

#define PROCESS_INVALID_ID          0     // Reserved (could mean "the kernel")

// Error codes (all negative, so any ProcessId > 0 is a success)
#define PROCESS_OK                  0
#define PROCESS_ERR_INVALID_ARG    -1
#define PROCESS_ERR_NO_SLOTS       -2
#define PROCESS_ERR_NO_MEMORY      -3
#define PROCESS_ERR_NO_PAGING      -4
#define PROCESS_ERR_LOAD_FAILED    -5
#define PROCESS_ERR_NOT_FOUND      -6
#define PROCESS_ERR_THREAD         -7

typedef enum {
    PROCESS_STATE_FREE,         // Slot unused
    PROCESS_STATE_CREATING,     // Slot reserved, being built
    PROCESS_STATE_RUNNING,      // Fully constructed, threads scheduled
    PROCESS_STATE_DYING         // Being destructed
} ProcessState;

// Read-only snapshot for debugging / task list.
typedef struct {
    ProcessId pid;
    ProcessState state;
    ThreadPriority priority;
    char name[PROCESS_NAME_MAX];
    uint32_t thread_count;
    uint32_t code_size;
    uint32_t data_size;
    uint32_t bss_size;
} ProcessInfo;

// Create a process from the executable at image_path. The main thread starts
// at the given priority. Returns a ProcessId (> 0) or a PROCESS_ERR_* code.
//
// The executable loader is not implemented yet: the path is recorded, the
// code/data/bss segments are left empty and the main thread is parked.
ProcessId CreateProcess(const char* image_path, ThreadPriority priority);

// Kill every thread of the process and free its address space and segments.
// Safe to call on the calling thread's own process (does not return then).
// Returns PROCESS_OK or a PROCESS_ERR_* code.
int DestroyProcess(ProcessId pid);

// Fill *info for a live process. Returns false if pid is not valid.
bool ProcessGetInfo(ProcessId pid, ProcessInfo* info);

// Number of live (creating or running) processes
uint32_t process_get_count(void);

#endif

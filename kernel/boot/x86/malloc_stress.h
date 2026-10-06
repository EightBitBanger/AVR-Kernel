#ifndef _KERNEL_MALLOC_STRESS_H_
#define _KERNEL_MALLOC_STRESS_H_

#include <stdint.h>
#include <stdbool.h>
#include <kernel/mutex.h>

//
// Heap stress test thread
//
// Hammers malloc/free with a mix of small, medium and large blocks in
// patterns designed to fragment the heap (fill, swiss-cheese, random churn,
// drain in varying orders). Every block is filled with a tag-derived pattern
// and verified before it is freed, so overlapping allocations, allocator
// metadata scribbling over live blocks, or double hand-outs are caught.
//
// The test itself never leaks: every cycle ends with all of its blocks freed
// and its own accounting checked to be zero. It also probes the largest
// allocatable block after each cycle and compares it to a baseline taken at
// start-up, which flags allocator leaks or failure to coalesce.
//
// All knobs can be overridden with -D on the compiler command line.

#ifndef MALLOC_STRESS_SLOTS
#define MALLOC_STRESS_SLOTS            256                  // Max blocks held at once
#endif

#ifndef MALLOC_STRESS_LIVE_BUDGET
#define MALLOC_STRESS_LIVE_BUDGET      (1024U * 1024U)      // Max bytes held at once (heap is 4 MB)
#endif

#ifndef MALLOC_STRESS_CHURN_OPS
#define MALLOC_STRESS_CHURN_OPS        4096                 // Random alloc/free ops per cycle
#endif

#ifndef MALLOC_STRESS_BATCH_OPS
#define MALLOC_STRESS_BATCH_OPS        32                   // Ops between pauses
#endif

#ifndef MALLOC_STRESS_PAUSE_MS
#define MALLOC_STRESS_PAUSE_MS         1                    // Sleep between batches (0 = yield)
#endif

#ifndef MALLOC_STRESS_CYCLE_PAUSE_MS
#define MALLOC_STRESS_CYCLE_PAUSE_MS   100                  // Sleep between full cycles
#endif

#ifndef MALLOC_STRESS_START_DELAY_MS
#define MALLOC_STRESS_START_DELAY_MS   2000                 // Let the DWM settle before the baseline
#endif

// Largest-free-block probe. Set to 0 if your malloc panics instead of
// returning NULL when a request can't be satisfied.
#ifndef MALLOC_STRESS_PROBE
#define MALLOC_STRESS_PROBE            1
#endif

#ifndef MALLOC_STRESS_PROBE_MAX
#define MALLOC_STRESS_PROBE_MAX        (4U * 1024U * 1024U) // Upper bound of the probe (heap size)
#endif

#ifndef MALLOC_STRESS_PROBE_GRAIN
#define MALLOC_STRESS_PROBE_GRAIN      256                  // Probe resolution in bytes
#endif

// How far the largest free block may sit below the baseline after a full
// drain before the cycle counts as a suspected leak. Other threads (DWM,
// kernel events) may legitimately hold memory, hence the slack.
#ifndef MALLOC_STRESS_LEAK_TOLERANCE
#define MALLOC_STRESS_LEAK_TOLERANCE   (64U * 1024U)
#endif

// Panic after this many consecutive suspect cycles (0 = only record it)
#ifndef MALLOC_STRESS_PANIC_ON_LEAK
#define MALLOC_STRESS_PANIC_ON_LEAK    0
#endif

// Print a one-line summary per cycle. Off by default because the DWM owns
// the screen once it is running.
#ifndef MALLOC_STRESS_PRINT
#define MALLOC_STRESS_PRINT            0
#endif

struct MallocStressStats {
    volatile uint32_t cycles;
    volatile uint32_t allocations;
    volatile uint32_t frees;
    volatile uint32_t failed_allocations;   // malloc returned NULL (heap full/fragmented)
    volatile uint32_t live_blocks;
    volatile uint32_t live_bytes;
    volatile uint32_t peak_live_bytes;
    volatile uint32_t verify_passes;
    volatile uint32_t misaligned_blocks;    // Pointers not 4-byte aligned
    volatile uint32_t baseline_largest_block;
    volatile uint32_t last_largest_block;
    volatile uint32_t min_largest_block;
    volatile uint32_t suspect_leak_cycles;  // Consecutive cycles below baseline - tolerance
};

extern struct MallocStressStats malloc_stress_stats;

// The mutex every other malloc/free caller holds (kernel_big_lock).
// Must be set before the thread is created.
void malloc_stress_set_lock(mutex_t* lock);

// Thread entry point for thread_create()
void malloc_stress_thread_main(void);

#endif

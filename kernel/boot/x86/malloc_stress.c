#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <kernel/boot/x86/malloc_stress.h>

#include <kernel/memory/malloc.h>
#include <kernel/scheduler/scheduler.h>
#include <kernel/mutex.h>
#include <kernel/util/timer.h>
#include <kernel/panic/panic_error.h>

#if MALLOC_STRESS_PRINT
#include <kernel/console/print.h>
#endif

//
// Heap stress test thread
//
// One cycle:
//   1. Fill       allocate mixed sizes until the slot table or byte budget is full
//   2. Cheese     free every other block, then try to put bigger blocks in the
//                 holes (most won't fit, which forces the allocator to search
//                 and split around many small free fragments)
//   3. Churn      random alloc / free / resize operations
//   4. Drain      free everything, in forward, reverse or random order
//                 (rotates per cycle, since coalescing bugs often depend on order)
//   5. Check      own accounting must be zero; probe the largest free block
//                 and compare it against the start-up baseline
//
// Every block is filled with a pattern derived from its tag and verified
// before it is freed, and the whole live set is verified at phase ends.
//
// The heap is shared with the rest of the kernel, so every malloc/free (and
// every touch of a live block's memory, so verification sees a consistent
// heap) happens while holding the same lock the DWM and kernel threads use.
// The lock is released between short batches so the desktop stays responsive.

struct StressSlot {
    uint8_t* ptr;
    uint32_t size;
    uint32_t tag;
};

struct MallocStressStats malloc_stress_stats;

static struct StressSlot slots[MALLOC_STRESS_SLOTS];
static uint16_t          drain_order[MALLOC_STRESS_SLOTS];
static mutex_t*          heap_lock = NULL;
static uint32_t          next_tag = 1;
static uint32_t          batch_ops = 0;
static uint32_t          rng_state = 0;

//
// Helpers
//

void malloc_stress_set_lock(mutex_t* lock) {
    heap_lock = lock;
}

// xorshift32: private PRNG so the test does not disturb the system RNG and
// stays deterministic for a given seed
static uint32_t stress_rand(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static uint32_t stress_rand_range(uint32_t lo, uint32_t hi) {
    return lo + (stress_rand() % (hi - lo + 1));
}

// Size mix: mostly small objects, some medium, a few large ones. The mix of
// tiny and large blocks is what produces fragmentation.
static uint32_t stress_pick_size(void) {
    uint32_t roll = stress_rand() % 100;
    if (roll < 55) return stress_rand_range(1, 64);
    if (roll < 85) return stress_rand_range(65, 1024);
    if (roll < 98) return stress_rand_range(1025, 16 * 1024);
    return stress_rand_range(16 * 1024 + 1, 64 * 1024);
}

static inline uint8_t stress_pattern(uint32_t tag, uint32_t i) {
    uint32_t v = tag * 0x9E3779B1U;
    return (uint8_t)((v >> 24) ^ (v >> 8) ^ i ^ (i >> 8) ^ (i >> 16));
}

static void stress_fail(uint32_t slot, uint32_t address, const char* what) {
    kernel_crashout(slot, address, PT_CPU_EXCEPTION, what);
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

static void stress_fill(struct StressSlot* s) {
    for (uint32_t i = 0; i < s->size; i++)
        s->ptr[i] = stress_pattern(s->tag, i);
}

static void stress_verify(uint32_t index) {
    struct StressSlot* s = &slots[index];
    for (uint32_t i = 0; i < s->size; i++) {
        if (s->ptr[i] != stress_pattern(s->tag, i))
            stress_fail(index, (uint32_t)&s->ptr[i], "HEAP CORRUPTION (MALLOC STRESS)");
    }
    malloc_stress_stats.verify_passes++;
}

static void stress_verify_all(void) {
    for (uint32_t i = 0; i < MALLOC_STRESS_SLOTS; i++) {
        if (slots[i].ptr)
            stress_verify(i);
    }
}

// Release the lock and let everyone else run every MALLOC_STRESS_BATCH_OPS
// heap operations. Must be called with the lock held; returns with it held.
static void stress_tick(void) {
    if (++batch_ops < MALLOC_STRESS_BATCH_OPS)
        return;
    batch_ops = 0;

    mutex_unlock(heap_lock);
    thread_sleep(MALLOC_STRESS_PAUSE_MS);
    mutex_lock(heap_lock);
}

// Allocate into an empty slot. Returns false if over budget or malloc failed.
static bool stress_alloc(uint32_t index, uint32_t size) {
    struct StressSlot* s = &slots[index];

    if (malloc_stress_stats.live_bytes + size > MALLOC_STRESS_LIVE_BUDGET)
        return false;

    uint8_t* p = (uint8_t*)malloc(size);
    stress_tick();

    if (!p) {
        malloc_stress_stats.failed_allocations++;
        return false;
    }

    if (((uint32_t)p & 3U) != 0)
        malloc_stress_stats.misaligned_blocks++;

    s->ptr  = p;
    s->size = size;
    s->tag  = next_tag++;
    if (next_tag == 0) next_tag = 1;
    stress_fill(s);

    malloc_stress_stats.allocations++;
    malloc_stress_stats.live_blocks++;
    malloc_stress_stats.live_bytes += size;
    if (malloc_stress_stats.live_bytes > malloc_stress_stats.peak_live_bytes)
        malloc_stress_stats.peak_live_bytes = malloc_stress_stats.live_bytes;
    return true;
}

static void stress_free(uint32_t index) {
    struct StressSlot* s = &slots[index];
    if (!s->ptr) return;

    stress_verify(index);

    // Scribble before freeing so a later use-after-free read of this block by
    // anyone would see garbage rather than plausible data
    for (uint32_t i = 0; i < s->size; i++)
        s->ptr[i] = 0xDD;

    free(s->ptr);

    malloc_stress_stats.frees++;
    malloc_stress_stats.live_blocks--;
    malloc_stress_stats.live_bytes -= s->size;

    s->ptr  = NULL;
    s->size = 0;
    s->tag  = 0;

    stress_tick();
}

//
// Phases
//

static void phase_fill(void) {
    for (uint32_t i = 0; i < MALLOC_STRESS_SLOTS; i++) {
        if (slots[i].ptr) continue;
        if (!stress_alloc(i, stress_pick_size()))
            break;
    }
}

static void phase_cheese(void) {
    // Punch holes: free every other block
    for (uint32_t i = 1; i < MALLOC_STRESS_SLOTS; i += 2)
        stress_free(i);

    stress_verify_all();

    // Refill the holes with blocks larger than what was there, so they mostly
    // can't reuse the hole they came from
    for (uint32_t i = 1; i < MALLOC_STRESS_SLOTS; i += 2) {
        uint32_t size = stress_pick_size() * 2 + 16;
        stress_alloc(i, size);
    }
}

static void phase_churn(void) {
    for (uint32_t op = 0; op < MALLOC_STRESS_CHURN_OPS; op++) {
        uint32_t index = stress_rand() % MALLOC_STRESS_SLOTS;
        uint32_t roll  = stress_rand() % 100;

        if (!slots[index].ptr) {
            stress_alloc(index, stress_pick_size());
        } else if (roll < 60) {
            stress_free(index);
        } else {
            // "Resize": free, then allocate a different size in the same slot
            uint32_t old_size = slots[index].size;
            stress_free(index);
            uint32_t new_size = (roll < 80) ? old_size / 2 + 1 : old_size * 2 + 1;
            if (new_size > 64 * 1024) new_size = 64 * 1024;
            stress_alloc(index, new_size);
        }
    }
}

static void phase_drain(uint32_t cycle) {
    for (uint32_t i = 0; i < MALLOC_STRESS_SLOTS; i++)
        drain_order[i] = (uint16_t)i;

    switch (cycle % 3) {
    case 0:     // Forward
        break;
    case 1:     // Reverse
        for (uint32_t i = 0; i < MALLOC_STRESS_SLOTS / 2; i++) {
            uint16_t t = drain_order[i];
            drain_order[i] = drain_order[MALLOC_STRESS_SLOTS - 1 - i];
            drain_order[MALLOC_STRESS_SLOTS - 1 - i] = t;
        }
        break;
    default:    // Random (Fisher-Yates)
        for (uint32_t i = MALLOC_STRESS_SLOTS - 1; i > 0; i--) {
            uint32_t j = stress_rand() % (i + 1);
            uint16_t t = drain_order[i];
            drain_order[i] = drain_order[j];
            drain_order[j] = t;
        }
        break;
    }

    for (uint32_t i = 0; i < MALLOC_STRESS_SLOTS; i++)
        stress_free(drain_order[i]);
}

#if MALLOC_STRESS_PROBE
// Binary search for the largest single allocation the heap can satisfy.
// Must be called with the lock held and none of our own blocks live.
static uint32_t probe_largest_block(void) {
    uint32_t lo = 0;                        // Known to fit
    uint32_t hi = MALLOC_STRESS_PROBE_MAX;  // Upper bound (may or may not fit)

    while (hi - lo > MALLOC_STRESS_PROBE_GRAIN) {
        uint32_t mid = lo + (hi - lo) / 2;
        void* p = malloc(mid);
        if (p) {
            free(p);
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}
#endif

#if MALLOC_STRESS_PRINT
static void print_u32(uint32_t v) {
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    do {
        buf[--i] = (char)('0' + (v % 10));
        v /= 10;
    } while (v && i > 0);
    print(&buf[i]);
}
#endif

//
// Thread entry
//

void malloc_stress_thread_main(void) {
    // No lock means nothing is protecting the heap from the other threads;
    // refuse to run rather than corrupt it.
    if (!heap_lock)
        return;

    thread_sleep(MALLOC_STRESS_START_DELAY_MS);

    rng_state = (uint32_t)timer_get_ms() ^ 0xA5F00D5EU;
    if (rng_state == 0) rng_state = 0x12345678U;

    for (uint32_t i = 0; i < MALLOC_STRESS_SLOTS; i++) {
        slots[i].ptr  = NULL;
        slots[i].size = 0;
        slots[i].tag  = 0;
    }

#if MALLOC_STRESS_PROBE
    mutex_lock(heap_lock);
    uint32_t baseline = probe_largest_block();
    mutex_unlock(heap_lock);

    malloc_stress_stats.baseline_largest_block = baseline;
    malloc_stress_stats.last_largest_block     = baseline;
    malloc_stress_stats.min_largest_block      = baseline;
#endif

    for (uint32_t cycle = 0; ; cycle++) {
        mutex_lock(heap_lock);
        batch_ops = 0;

        phase_fill();
        stress_verify_all();

        phase_cheese();
        stress_verify_all();

        phase_churn();
        stress_verify_all();

        phase_drain(cycle);

        // Our own books must balance after a full drain
        if (malloc_stress_stats.live_blocks != 0 || malloc_stress_stats.live_bytes != 0)
            stress_fail(malloc_stress_stats.live_blocks, malloc_stress_stats.live_bytes,
                        "MALLOC STRESS ACCOUNTING");

#if MALLOC_STRESS_PROBE
        uint32_t largest = probe_largest_block();
        malloc_stress_stats.last_largest_block = largest;
        if (largest < malloc_stress_stats.min_largest_block)
            malloc_stress_stats.min_largest_block = largest;

        // Everything we allocated is freed, so the heap should be able to
        // hand back roughly the same contiguous block as at start-up. If it
        // can't, the allocator leaked blocks or failed to coalesce.
        if (largest + MALLOC_STRESS_LEAK_TOLERANCE < malloc_stress_stats.baseline_largest_block) {
            malloc_stress_stats.suspect_leak_cycles++;
#if MALLOC_STRESS_PANIC_ON_LEAK
            if (malloc_stress_stats.suspect_leak_cycles >= MALLOC_STRESS_PANIC_ON_LEAK)
                stress_fail(largest, malloc_stress_stats.baseline_largest_block,
                            "MALLOC STRESS: HEAP LEAK OR NO COALESCING");
#endif
        } else {
            malloc_stress_stats.suspect_leak_cycles = 0;
        }
#endif

        mutex_unlock(heap_lock);

        malloc_stress_stats.cycles++;

#if MALLOC_STRESS_PRINT
        print("malloc stress: cycle ");
        print_u32(malloc_stress_stats.cycles);
        print(" allocs ");
        print_u32(malloc_stress_stats.allocations);
        print(" fails ");
        print_u32(malloc_stress_stats.failed_allocations);
        print(" largest ");
        print_u32(malloc_stress_stats.last_largest_block);
        print("\n");
#endif

        thread_sleep(MALLOC_STRESS_CYCLE_PAUSE_MS);
    }
}

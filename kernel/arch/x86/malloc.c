#include <kernel/arch/x86/malloc.h>
#include <kernel/util/string.h>

#define NUM_BUCKETS 6
#define MAX_SLAB_SIZE 512

static struct SlabCache malloc_caches[] = {
    { .object_size = 16,             .page_list = NULL },
    { .object_size = 32,             .page_list = NULL },
    { .object_size = 64,             .page_list = NULL },
    { .object_size = 128,            .page_list = NULL },
    { .object_size = 256,            .page_list = NULL },
    { .object_size = MAX_SLAB_SIZE,  .page_list = NULL }
};


struct LargeAllocHeader {
    size_t num_pages;
    uint32_t magic;
};

void* malloc(size_t size) {
    if (size == 0) return NULL;
    
    // Find a suitable slab
    if (size <= MAX_SLAB_SIZE) {
        for (int i = 0; i < sizeof(malloc_caches) / sizeof( struct SlabCache); i++) {
            if (size <= malloc_caches[i].object_size) {
                return slab_alloc(&malloc_caches[i]);
            }
        }
    }
    
    // Size is larger than the largest slab
    
    // We need 'size' bytes, plus 1 entire page at the front to hold our metadata safely
    size_t total_size = size + PAGE_SIZE;
    size_t num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    size_t* raw_mem = (size_t*)vmm_alloc_pages(num_pages);
    if (!raw_mem) return NULL;
    
    memset(raw_mem, 0, num_pages * PAGE_SIZE);
    
    // Store the total number of pages at the very beginning of the first page
    raw_mem[0] = num_pages;
    
    // Return the start of the SECOND page. This pointer is perfectly page-aligned!
    return (void*)((uint8_t*)raw_mem + PAGE_SIZE);
}

void free(void* ptr) {
    if (!ptr) return;
    
    // Check if the pointer perfectly page-aligned
    // A standard slab slot is never page-aligned due to the pre appended SlabPage header
    if (((uint32_t)ptr & (PAGE_SIZE - 1)) == 0) {
        // It's a large allocation. The metadata page is exactly 1 page backward
        void* raw_page_start = (void*)((uint8_t*)ptr - PAGE_SIZE);
        size_t pages_to_free = *(size_t*)raw_page_start;
        
        vmm_free_pages(raw_page_start, pages_to_free);
        return;
    }
    
    // Standard Slab Free path
    uint32_t page_base = (uint32_t)ptr & ~(PAGE_SIZE - 1);
    struct SlabPage* page = (struct SlabPage*)page_base;
    struct SlabCache* cache = page->owning_cache;
    
    slab_free(cache, ptr);
}


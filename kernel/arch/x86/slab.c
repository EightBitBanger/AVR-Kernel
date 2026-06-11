#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <kernel/util/string.h>
#include <kernel/arch/x86/virtual/vmm.h>
#include <kernel/panic/panic_error.h>

#include <kernel/arch/x86/slab.h>

struct FreeNode {
    struct FreeNode* next;
};

struct SlabPage {
    struct FreeNode* free_list; // Points to the first available slot in this page
    size_t num_allocated;       // How many slots are currently active
    struct SlabPage* next;      // Link to the next page in the cache
};


static struct SlabPage* slab_grow(size_t object_size) {
    // Get a fresh 4KB page from your VMM
    void* raw_page = vmm_alloc_pages(1);
    if (!raw_page) return NULL;
    
    memset(raw_page, 0, PAGE_SIZE);
    
    // Place the SlabPage header at the very beginning of the page
    struct SlabPage* page = (struct SlabPage*)raw_page;
    page->num_allocated = 0;
    page->next = NULL;
    
    // Calculate where the slots start (right after the header)
    // We align the start address to a 4-byte boundary for safety
    uint32_t header_size = sizeof(struct SlabPage);
    uint32_t start_offset = (header_size + 3) & ~3U; 
    
    uint8_t* page_bytes = (uint8_t*)raw_page;
    uint8_t* first_slot = page_bytes + start_offset;
    
    // Enforce a minimum size so a pointer can actually fit inside the slot
    if (object_size < sizeof(struct FreeNode)) {
        object_size = sizeof(struct FreeNode);
    }
    
    // Thread the slots together into the free list
    page->free_list = (struct FreeNode*)first_slot;
    struct FreeNode* current = page->free_list;
    
    uint32_t max_slots = (PAGE_SIZE - start_offset) / object_size;
    
    for (uint32_t i = 0; i < max_slots - 1; i++) {
        struct FreeNode* next_node = (struct FreeNode*)((uint8_t*)current + object_size);
        current->next = next_node; // Point current slot to the next slot
        current = next_node;
    }
    
    // The very last slot terminates the list
    current->next = NULL;
    
    return page;
}

void* slab_alloc(SlabCache* cache) {
    struct SlabPage* page = cache->page_list;
    
    // Find a page that has at least one free slot
    while (page != NULL && page->free_list == NULL) {
        page = page->next;
    }
    
    // If all pages are full (or we have no pages), grow the cache
    if (!page) {
        page = slab_grow(cache->object_size);
        if (!page) return NULL; // Out of memory!
        
        // Push the new page to the front of our cache list
        page->next = cache->page_list;
        cache->page_list = page;
    }
    
    // Pop the first free slot off the embedded list
    struct FreeNode* allocated_slot = page->free_list;
    page->free_list = allocated_slot->next;
    page->num_allocated++;
    
    // Return the slot address directly to the user
    return (void*)allocated_slot;
}

void slab_free(SlabCache* cache, void* ptr) {
    if (!ptr) return;
    
    // Find the base address of the 4KB page containing this pointer
    uint32_t page_base = (uint32_t)ptr & ~(PAGE_SIZE - 1);
    struct SlabPage* page = (struct SlabPage*)page_base;
    
    // Cast the returned memory back into a FreeNode
    struct FreeNode* node = (struct FreeNode*)ptr;
    
    // Push this slot back onto the front of this page's free list
    node->next = page->free_list;
    page->free_list = node;
    page->num_allocated--;
    
    // OPTIONAL: If page->num_allocated hitting 0, you could call vmm_free_pages
    // to return the whole 4KB page back to the VMM system if you want to avoid hoarding memory.
}

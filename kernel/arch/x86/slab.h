#ifndef KERNEL_MEMORY_SLAB_ALLOCATOR_H
#define KERNEL_MEMORY_SLAB_ALLOCATOR_H

typedef struct {
    size_t object_size;         // Size of objects this cache handles
    struct SlabPage* page_list; // Linked list of all pages in this cache
} SlabCache;

void* slab_alloc(SlabCache* cache);

void slab_free(SlabCache* cache, void* ptr);

#endif

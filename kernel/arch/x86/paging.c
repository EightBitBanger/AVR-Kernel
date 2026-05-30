#include <stdint.h>
#include <stdbool.h>
#include <kernel/util/string.h>

#include <kernel/arch/x86/heap.h>

static uint32_t kmalloc_block_size       = 0;
static uint32_t kmalloc_pool_size        = 0;
static uint32_t kmalloc_block_count      = 0;

static uint32_t kmalloc_bitmap_size      = 0;
static uint32_t kmalloc_reserved_blocks  = 0;

#define KMALLOC_MAGIC        0x55U
#define KMALLOC_HEADER_SIZE  ((uint32_t)sizeof(struct KMallocHeader))

uint8_t* kmalloc_bitmap = 0;
uint8_t* kmalloc_dirty = 0;

uint32_t __KMALLOC_HEAP_BEGIN__;

static inline uint32_t kmalloc_heap_data_begin(void) {
    return kmalloc_reserved_blocks * kmalloc_block_size;
}

void kmalloc_bitmap_set(uint8_t* bitmap, uint8_t* bitmap_dirty) {
    kmalloc_bitmap = bitmap;
    kmalloc_dirty = bitmap_dirty;
    
    if (kmalloc_dirty != 0) {
        memset(kmalloc_dirty, 0, kmalloc_bitmap_size);
    }
}

void heap_set_base_address(uint32_t base) {
    __KMALLOC_HEAP_BEGIN__ = base;
}

uint32_t heap_get_base_address(void) {
    return __KMALLOC_HEAP_BEGIN__;
}

static inline void kmalloc_bitmap_mark_dirty(uint32_t byte_index) {
    if (kmalloc_dirty != 0) {
        kmalloc_dirty[byte_index] = 1;
    }
}

void kmalloc_bitmap_write(void) {
    if (kmalloc_bitmap == 0)
        return;
    
    uint8_t* dest_bitmap = (uint8_t*)(uintptr_t)__KMALLOC_HEAP_BEGIN__;
    
    if (kmalloc_dirty == 0) {
        memcpy(dest_bitmap, kmalloc_bitmap, kmalloc_bitmap_size);
        return;
    }
    
    for (uint32_t index = 0; index < kmalloc_bitmap_size; index++) {
        if (kmalloc_dirty[index] == 0)
            continue;
        
        dest_bitmap[index] = kmalloc_bitmap[index];
        kmalloc_dirty[index] = 0;
    }
}

void kmalloc_bitmap_read(void) {
    if (kmalloc_bitmap == 0)
        return;
    
    uint8_t* src_bitmap = (uint8_t*)(uintptr_t)__KMALLOC_HEAP_BEGIN__;
    memcpy(kmalloc_bitmap, src_bitmap, kmalloc_bitmap_size);
    
    if (kmalloc_dirty != 0) {
        memset(kmalloc_dirty, 0, kmalloc_bitmap_size);
    }
}

static inline void kmem_write8(uint32_t address, uint8_t value) {
    *(volatile uint8_t*)(uintptr_t)(__KMALLOC_HEAP_BEGIN__ + address) = value;
}

static inline uint8_t kmem_read8(uint32_t address) {
    return *(volatile uint8_t*)(uintptr_t)(__KMALLOC_HEAP_BEGIN__ + address);
}

static inline void kmalloc_header_write(uint32_t address, const struct KMallocHeader* header) {
    volatile struct KMallocHeader* dest = (struct KMallocHeader*)(uintptr_t)(__KMALLOC_HEAP_BEGIN__ + address);
    *dest = *header;
}

static inline void kmalloc_header_read(uint32_t address, struct KMallocHeader* header) {
    volatile struct KMallocHeader* src = (struct KMallocHeader*)(uintptr_t)(__KMALLOC_HEAP_BEGIN__ + address);
    *header = *src;
}

static inline uint32_t kmalloc_blocks_required(uint32_t size) {
    if (size == 0)
        return 0;
    return (size + (kmalloc_block_size - 1UL)) / kmalloc_block_size;
}

static inline bool kmalloc_block_is_used(uint32_t block_index) {
    uint32_t byte_index = block_index / 8UL;
    uint8_t  bit_index  = (uint8_t)(block_index % 8UL);
    return (kmalloc_bitmap[byte_index] & (1U << bit_index)) != 0;
}

static inline void kmalloc_block_set_used(uint32_t block_index) {
    uint32_t byte_index = block_index / 8UL;
    uint8_t  bit_index  = (uint8_t)(block_index % 8UL);
    uint8_t  old_value  = kmalloc_bitmap[byte_index];
    uint8_t  new_value  = (uint8_t)(old_value | (1U << bit_index));
    
    if (new_value != old_value) {
        kmalloc_bitmap[byte_index] = new_value;
        kmalloc_bitmap_mark_dirty(byte_index);
    }
}

static inline void kmalloc_block_set_free(uint32_t block_index) {
    uint32_t byte_index = block_index / 8UL;
    uint8_t  bit_index  = (uint8_t)(block_index % 8UL);
    uint8_t  old_value  = kmalloc_bitmap[byte_index];
    uint8_t  new_value  = (uint8_t)(old_value & ~(1U << bit_index));
    
    if (new_value != old_value) {
        kmalloc_bitmap[byte_index] = new_value;
        kmalloc_bitmap_mark_dirty(byte_index);
    }
}

bool kmalloc_is_valid(uint32_t address) {
    if (address == KMALLOC_NULL)
        return false;
    
    // Bounds check relative to absolute addresses
    if (address < (__KMALLOC_HEAP_BEGIN__ + kmalloc_heap_data_begin() + KMALLOC_HEADER_SIZE))
        return false;
    
    if (address >= (__KMALLOC_HEAP_BEGIN__ + kmalloc_pool_size))
        return false;
    
    uint32_t header_address_abs = address - KMALLOC_HEADER_SIZE;
    uint32_t header_address_rel = header_address_abs - __KMALLOC_HEAP_BEGIN__;
    
    if (header_address_rel < kmalloc_heap_data_begin() || header_address_abs >= (__KMALLOC_HEAP_BEGIN__ + kmalloc_pool_size))
        return false;
    
    struct KMallocHeader header;
    kmalloc_header_read(header_address_rel, &header);
    
    if (header.magic != KMALLOC_MAGIC || header.size == 0)
        return false;
    
    uint32_t total_size      = header.size + KMALLOC_HEADER_SIZE;
    uint32_t required_blocks = kmalloc_blocks_required(total_size);
    uint32_t start_block     = header_address_rel / kmalloc_block_size;
    
    if (required_blocks == 0 || start_block < kmalloc_reserved_blocks)
        return false;
    
    if ((start_block + required_blocks) > kmalloc_block_count)
        return false;
    
    return true;
}

uint32_t heap_get_block_size(void) {
    return kmalloc_block_size;
}

uint32_t heap_get_total_memory(void) {
    return kmalloc_pool_size;
}

void heap_init(uint32_t block_size, uint32_t total_memory) {
    if (block_size == 0 || total_memory == 0) {
        kmalloc_block_size      = 0;
        kmalloc_pool_size       = 0;
        kmalloc_block_count     = 0;
        kmalloc_bitmap_size     = 0;
        kmalloc_reserved_blocks = 0;
        return;
    }
    kmalloc_block_size   = block_size;
    kmalloc_pool_size    = total_memory;
    kmalloc_block_count  = kmalloc_pool_size / kmalloc_block_size;
    
    kmalloc_bitmap_size  = (kmalloc_block_count + 7UL) / 8UL;
    
    kmalloc_bitmap = (uint8_t*)(uintptr_t)__KMALLOC_HEAP_BEGIN__;
    kmalloc_dirty  = kmalloc_bitmap + kmalloc_bitmap_size;
    
    uint32_t total_overhead_bytes = kmalloc_bitmap_size * 2;
    
    kmalloc_reserved_blocks = (total_overhead_bytes + kmalloc_block_size - 1UL) / kmalloc_block_size;
    
    memset(kmalloc_bitmap, 0, kmalloc_bitmap_size);
    memset(kmalloc_dirty,  0, kmalloc_bitmap_size);
    
    kmalloc_bitmap_write();
}

uint32_t kmalloc(uint32_t size) {
    if (size == 0)
        return KMALLOC_NULL;
    
    if (kmalloc_block_size == 0 || kmalloc_pool_size == 0 || kmalloc_bitmap == 0)
        return KMALLOC_NULL;
    
    uint32_t total_size      = size + KMALLOC_HEADER_SIZE;
    uint32_t required_blocks = kmalloc_blocks_required(total_size);
    
    if (required_blocks == 0 || required_blocks > (kmalloc_block_count - kmalloc_reserved_blocks))
        return KMALLOC_NULL;
    
    uint32_t found_blocks = 0;
    uint32_t start_block  = 0;
    
    for (uint32_t current_block = kmalloc_reserved_blocks; current_block < kmalloc_block_count; current_block++) {
        if (!kmalloc_block_is_used(current_block)) {
            if (found_blocks == 0)
                start_block = current_block;
            
            found_blocks++;
            
            if (found_blocks >= required_blocks) {
                for (uint32_t block_offset = 0; block_offset < required_blocks; block_offset++) {
                    kmalloc_block_set_used(start_block + block_offset);
                }
                
                uint32_t header_address_rel = start_block * kmalloc_block_size;
                
                struct KMallocHeader header = {
                    .size  = size,
                    .flags = 0,
                    .type  = 0,
                    .perm  = 0,
                    .magic = KMALLOC_MAGIC
                };
                
                kmalloc_header_write(header_address_rel, &header);
                
                // Flush changes to the physical bitmap tracking layer
                kmalloc_bitmap_write();
                
                // CRITICAL: Return absolute virtual/physical address
                return __KMALLOC_HEAP_BEGIN__ + header_address_rel + KMALLOC_HEADER_SIZE;
            }
        } else {
            found_blocks = 0;
        }
    }
    
    return KMALLOC_NULL;
}

void kfree(uint32_t address) {
    if (!kmalloc_is_valid(address))
        return;
    
    uint32_t header_address_abs = address - KMALLOC_HEADER_SIZE;
    uint32_t header_address_rel = header_address_abs - __KMALLOC_HEAP_BEGIN__;
    
    struct KMallocHeader header;
    kmalloc_header_read(header_address_rel, &header);
    
    uint32_t total_size      = header.size + KMALLOC_HEADER_SIZE;
    uint32_t required_blocks = kmalloc_blocks_required(total_size);
    uint32_t start_block     = header_address_rel / kmalloc_block_size;
    
    if (start_block < kmalloc_reserved_blocks || (start_block + required_blocks) > kmalloc_block_count)
        return;
    
    memset(&header, 0, sizeof(struct KMallocHeader));
    kmalloc_header_write(header_address_rel, &header);
    
    for (uint32_t block_offset = 0; block_offset < required_blocks; block_offset++) {
        kmalloc_block_set_free(start_block + block_offset);
    }
    
    // Sync backing structure changes
    kmalloc_bitmap_write();
}

void* malloc(size_t size) {
    uint32_t addr = kmalloc((uint32_t)size);
    
    if (addr == KMALLOC_NULL) 
        return NULL;
    
    return (void*)(uintptr_t)addr;
}

void free(void* ptr) {
    if (ptr == NULL) 
        return;
    
    uint32_t addr = (uint32_t)(uintptr_t)ptr;
    kfree(addr);
}

// Helpers updated to utilize fixed layout logic
uint8_t kmalloc_get_type(uint32_t address) {
    if (!kmalloc_is_valid(address)) return 0;
    struct KMallocHeader header;
    kmalloc_header_read((address - KMALLOC_HEADER_SIZE) - __KMALLOC_HEAP_BEGIN__, &header);
    return header.type;
}

void kmalloc_set_type(uint32_t address, uint8_t type) {
    if (!kmalloc_is_valid(address)) return;
    struct KMallocHeader header;
    uint32_t header_address_rel = (address - KMALLOC_HEADER_SIZE) - __KMALLOC_HEAP_BEGIN__;
    kmalloc_header_read(header_address_rel, &header);
    header.type = type;
    kmalloc_header_write(header_address_rel, &header);
}

uint8_t kmalloc_get_flags(uint32_t address) {
    if (!kmalloc_is_valid(address)) return 0;
    struct KMallocHeader header;
    kmalloc_header_read((address - KMALLOC_HEADER_SIZE) - __KMALLOC_HEAP_BEGIN__, &header);
    return header.flags;
}

void kmalloc_set_flags(uint32_t address, uint8_t flags) {
    if (!kmalloc_is_valid(address)) return;
    struct KMallocHeader header;
    uint32_t header_address_rel = (address - KMALLOC_HEADER_SIZE) - __KMALLOC_HEAP_BEGIN__;
    kmalloc_header_read(header_address_rel, &header);
    header.flags = flags;
    kmalloc_header_write(header_address_rel, &header);
}

uint8_t kmalloc_get_permissions(uint32_t address) {
    if (!kmalloc_is_valid(address)) return 0;
    struct KMallocHeader header;
    kmalloc_header_read((address - KMALLOC_HEADER_SIZE) - __KMALLOC_HEAP_BEGIN__, &header);
    return header.perm;
}

void kmalloc_set_permissions(uint32_t address, uint8_t permissions) {
    if (!kmalloc_is_valid(address)) return;
    struct KMallocHeader header;
    uint32_t header_address_rel = (address - KMALLOC_HEADER_SIZE) - __KMALLOC_HEAP_BEGIN__;
    kmalloc_header_read(header_address_rel, &header);
    header.perm = permissions;
    kmalloc_header_write(header_address_rel, &header);
}

uint32_t kmalloc_get_size(uint32_t address) {
    if (!kmalloc_is_valid(address)) return 0;
    struct KMallocHeader header;
    kmalloc_header_read((address - KMALLOC_HEADER_SIZE) - __KMALLOC_HEAP_BEGIN__, &header);
    return header.size;
}

uint32_t kmalloc_next(uint32_t previous_address) {
    uint32_t start_block;
    struct KMallocHeader header;
    
    if (previous_address == KMALLOC_NULL) {
        start_block = kmalloc_reserved_blocks;
    } else {
        if (!kmalloc_is_valid(previous_address))
            return KMALLOC_NULL;
        
        uint32_t header_address_rel = (previous_address - KMALLOC_HEADER_SIZE) - __KMALLOC_HEAP_BEGIN__;
        kmalloc_header_read(header_address_rel, &header);
        
        uint32_t total_size  = header.size + KMALLOC_HEADER_SIZE;
        uint32_t used_blocks = kmalloc_blocks_required(total_size);
        
        start_block = (header_address_rel / kmalloc_block_size) + used_blocks;
    }
    
    for (uint32_t current_block = start_block; current_block < kmalloc_block_count; current_block++) {
        if (!kmalloc_block_is_used(current_block))
            continue;
        
        uint32_t header_address_rel = current_block * kmalloc_block_size;
        if (header_address_rel < kmalloc_heap_data_begin())
            continue;
        
        kmalloc_header_read(header_address_rel, &header);
        
        if (header.magic != KMALLOC_MAGIC || header.size == 0)
            continue;
        
        if ((header_address_rel + KMALLOC_HEADER_SIZE) >= kmalloc_pool_size)
            continue;
        
        return __KMALLOC_HEAP_BEGIN__ + header_address_rel + KMALLOC_HEADER_SIZE;
    }
    
    return KMALLOC_NULL;
}

void kmem_write(uint32_t destination, const void* source, uint32_t size) {
    void* dest_ptr = (void*)(uintptr_t)(__KMALLOC_HEAP_BEGIN__ + destination);
    memcpy(dest_ptr, source, size);
}

void kmem_read(void* destination, uint32_t source, uint32_t size) {
    const void* src_ptr = (const void*)(uintptr_t)(__KMALLOC_HEAP_BEGIN__ + source);
    memcpy(destination, src_ptr, size);
}

void kmemset(uint32_t destination, unsigned char value, uint32_t size) {
    void* dest_ptr = (void*)(uintptr_t)(__KMALLOC_HEAP_BEGIN__ + destination);
    memset(dest_ptr, value, size);
}

#include <stdint.h>
#include <stdbool.h>

#include <kernel/arch/avr/io.h>
#include <kernel/boot/avr/heap.h>

#include <kernel/bus/bus.h>

static uint32_t kmalloc_block_size       = 0;
static uint32_t kmalloc_pool_size        = 0;
static uint32_t kmalloc_block_count      = 0;

static uint32_t kmalloc_bitmap_size      = 0;
static uint32_t kmalloc_reserved_blocks  = 0;

#define KMALLOC_MAGIC        0x55UL
#define KMALLOC_HEADER_SIZE  ((uint32_t)sizeof(struct KMallocHeader))

uint8_t* kmalloc_bitmap = 0;
uint8_t* kmalloc_dirty = 0;

uint32_t __KMALLOC_HEAP_BEGIN__ = 0;

static inline uint32_t kmalloc_heap_data_begin(void) {
    return kmalloc_reserved_blocks * kmalloc_block_size;
}

static inline uint32_t kmalloc_heap_data_size(void) {
    if (kmalloc_pool_size <= kmalloc_heap_data_begin())
        return 0;
    return kmalloc_pool_size - kmalloc_heap_data_begin();
}

void kmalloc_bitmap_set(uint8_t* bitmap, uint8_t* bitmap_dirty) {
    kmalloc_bitmap = bitmap;
    kmalloc_dirty = bitmap_dirty;
    
    if (kmalloc_dirty != 0) {
        for (uint32_t index = 0; index < kmalloc_bitmap_size; index++) {
            kmalloc_dirty[index] = 0;
        }
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
    struct Bus bus;
    uint32_t   index;
    
    bus.write_waitstate = 1;
    bus.read_waitstate  = 2;
    
    if (kmalloc_bitmap == 0)
        return;
    
    if (kmalloc_dirty == 0) {
        for (index = 0; index < kmalloc_bitmap_size; index++) {
            mmio_writeb(&bus, __KMALLOC_HEAP_BEGIN__ + index, &kmalloc_bitmap[index]);
        }
        return;
    }
    
    for (index = 0; index < kmalloc_bitmap_size; index++) {
        if (kmalloc_dirty[index] == 0)
            continue;
        
        mmio_writeb(&bus, __KMALLOC_HEAP_BEGIN__ + index, &kmalloc_bitmap[index]);
        kmalloc_dirty[index] = 0;
    }
}

void kmalloc_bitmap_read(void) {
    struct Bus bus;
    uint32_t   index;
    
    bus.write_waitstate = 1;
    bus.read_waitstate  = 2;
    
    if (kmalloc_bitmap == 0)
        return;
    
    for (index = 0; index < kmalloc_bitmap_size; index++) {
        mmio_readb(&bus, __KMALLOC_HEAP_BEGIN__ + index, &kmalloc_bitmap[index]);
    }
    
    if (kmalloc_dirty != 0) {
        for (index = 0; index < kmalloc_bitmap_size; index++) {
            kmalloc_dirty[index] = 0;
        }
    }
}

static void kmem_write8(uint32_t address, uint8_t value) {
    struct Bus bus;
    
    bus.write_waitstate = 1;
    bus.read_waitstate  = 2;
    
    mmio_writeb(&bus, __KMALLOC_HEAP_BEGIN__ + address, &value);
}

static uint8_t kmem_read8(uint32_t address) {
    struct Bus bus;
    uint8_t    byte;
    
    bus.write_waitstate = 1;
    bus.read_waitstate  = 2;
    
    mmio_readb(&bus, __KMALLOC_HEAP_BEGIN__ + address, &byte);
    
    return byte;
}

static void kmem_write32(uint32_t address, uint32_t value) {
    kmem_write8(address + 0, (uint8_t)((value >> 0)  & 0xFF));
    kmem_write8(address + 1, (uint8_t)((value >> 8)  & 0xFF));
    kmem_write8(address + 2, (uint8_t)((value >> 16) & 0xFF));
    kmem_write8(address + 3, (uint8_t)((value >> 24) & 0xFF));
}

static uint32_t kmem_read32(uint32_t address) {
    uint32_t value = 0;
    
    value |= ((uint32_t)kmem_read8(address + 0) << 0);
    value |= ((uint32_t)kmem_read8(address + 1) << 8);
    value |= ((uint32_t)kmem_read8(address + 2) << 16);
    value |= ((uint32_t)kmem_read8(address + 3) << 24);

    return value;
}

static void kmalloc_header_write(uint32_t address, const struct KMallocHeader* header) {
    kmem_write32(address + 0, header->size);
    kmem_write8 (address + 4, header->flags);
    kmem_write8 (address + 5, header->type);
    kmem_write8 (address + 6, header->perm);
    kmem_write8 (address + 7, header->magic);
}

static void kmalloc_header_read(uint32_t address, struct KMallocHeader* header) {
    header->size  = kmem_read32(address + 0);
    header->flags = kmem_read8(address + 4);
    header->type  = kmem_read8(address + 5);
    header->perm  = kmem_read8(address + 6);
    header->magic = kmem_read8(address + 7);
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
    uint8_t  old_value;
    uint8_t  new_value;
    
    old_value = kmalloc_bitmap[byte_index];
    new_value = (uint8_t)(old_value | (uint8_t)(1U << bit_index));
    
    if (new_value != old_value) {
        kmalloc_bitmap[byte_index] = new_value;
        kmalloc_bitmap_mark_dirty(byte_index);
    }
}

static inline void kmalloc_block_set_free(uint32_t block_index) {
    uint32_t byte_index = block_index / 8UL;
    uint8_t  bit_index  = (uint8_t)(block_index % 8UL);
    uint8_t  old_value;
    uint8_t  new_value;
    
    old_value = kmalloc_bitmap[byte_index];
    new_value = (uint8_t)(old_value & (uint8_t)~(uint8_t)(1U << bit_index));
    
    if (new_value != old_value) {
        kmalloc_bitmap[byte_index] = new_value;
        kmalloc_bitmap_mark_dirty(byte_index);
    }
}

bool kmalloc_is_valid(uint32_t address) {
    uint32_t             header_address;
    uint32_t             total_size;
    uint32_t             required_blocks;
    uint32_t             start_block;
    struct KMallocHeader header;
    
    if (address == KMALLOC_NULL)
        return false;
    
    if (address < (kmalloc_heap_data_begin() + KMALLOC_HEADER_SIZE))
        return false;
    
    if (address >= kmalloc_pool_size)
        return false;
    
    header_address = address - KMALLOC_HEADER_SIZE;
    
    if (header_address < kmalloc_heap_data_begin())
        return false;
    
    if (header_address >= kmalloc_pool_size)
        return false;
    
    kmalloc_header_read(header_address, &header);
    
    if (header.magic != KMALLOC_MAGIC)
        return false;
    
    if (header.size == 0)
        return false;
    
    total_size      = header.size + KMALLOC_HEADER_SIZE;
    required_blocks = kmalloc_blocks_required(total_size);
    start_block     = header_address / kmalloc_block_size;
    
    if (required_blocks == 0)
        return false;
    
    if (start_block < kmalloc_reserved_blocks)
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
    uint32_t index;
    
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
    
    kmalloc_bitmap_size     = (kmalloc_block_count + 7UL) / 8UL;
    kmalloc_reserved_blocks = (kmalloc_bitmap_size + kmalloc_block_size - 1UL) / kmalloc_block_size;
    
    if (kmalloc_bitmap == 0)
        return;
    
    for (index = 0; index < kmalloc_bitmap_size; index++) {
        kmalloc_bitmap[index] = 0;
    }
    
    if (kmalloc_dirty != 0) {
        for (index = 0; index < kmalloc_bitmap_size; index++) {
            kmalloc_dirty[index] = 0;
        }
    }
}

uint32_t kmalloc(uint32_t size) {
    uint32_t total_size;
    uint32_t required_blocks;
    uint32_t start_block;
    uint32_t current_block;
    uint32_t found_blocks;
    
    if (size == 0)
        return KMALLOC_NULL;
    
    if (kmalloc_block_size == 0 || kmalloc_pool_size == 0 || kmalloc_bitmap == 0)
        return KMALLOC_NULL;
    
    total_size      = size + KMALLOC_HEADER_SIZE;
    required_blocks = kmalloc_blocks_required(total_size);
    
    if (required_blocks == 0)
        return KMALLOC_NULL;
    
    if (required_blocks > (kmalloc_block_count - kmalloc_reserved_blocks))
        return KMALLOC_NULL;
    
    found_blocks = 0;
    start_block  = 0;
    
    for (current_block = kmalloc_reserved_blocks; current_block < kmalloc_block_count; current_block++) {
        if (!kmalloc_block_is_used(current_block)) {
            if (found_blocks == 0)
                start_block = current_block;
            
            found_blocks++;
            
            if (found_blocks >= required_blocks) {
                uint32_t             block_offset;
                uint32_t             header_address;
                uint32_t             payload_address;
                struct KMallocHeader header;
                
                for (block_offset = 0; block_offset < required_blocks; block_offset++) {
                    kmalloc_block_set_used(start_block + block_offset);
                }
                
                header_address  = start_block * kmalloc_block_size;
                payload_address = header_address + KMALLOC_HEADER_SIZE;
                
                header.size = size;
                header.flags = 0;
                header.type  = 0;
                header.perm  = 0;
                header.magic = KMALLOC_MAGIC;
                
                kmalloc_header_write(header_address, &header);
                
                return payload_address;
            }
        }
        else {
            found_blocks = 0;
        }
    }
    
    return KMALLOC_NULL;
}

void kfree(uint32_t address) {
    uint32_t             header_address;
    uint32_t             total_size;
    uint32_t             start_block;
    uint32_t             required_blocks;
    uint32_t             block_offset;
    struct KMallocHeader header;
    
    if (!kmalloc_is_valid(address))
        return;
    
    header_address = address - KMALLOC_HEADER_SIZE;
    kmalloc_header_read(header_address, &header);
    
    total_size      = header.size + KMALLOC_HEADER_SIZE;
    required_blocks = kmalloc_blocks_required(total_size);
    start_block     = header_address / kmalloc_block_size;
    
    if (start_block < kmalloc_reserved_blocks)
        return;
    
    if ((start_block + required_blocks) > kmalloc_block_count)
        return;
    
    header.size = 0;
    header.flags = 0;
    header.type  = 0;
    header.perm  = 0;
    header.magic = 0;
    
    kmalloc_header_write(header_address, &header);
    
    for (block_offset = 0; block_offset < required_blocks; block_offset++) {
        kmalloc_block_set_free(start_block + block_offset);
    }
}

uint8_t kmalloc_get_type(uint32_t address) {
    uint32_t             header_address;
    struct KMallocHeader header;
    
    if (!kmalloc_is_valid(address))
        return 0;
    
    header_address = address - KMALLOC_HEADER_SIZE;
    kmalloc_header_read(header_address, &header);
    
    return header.type;
}

void kmalloc_set_type(uint32_t address, uint8_t type) {
    uint32_t             header_address;
    struct KMallocHeader header;
    
    if (!kmalloc_is_valid(address))
        return;
    
    header_address = address - KMALLOC_HEADER_SIZE;
    kmalloc_header_read(header_address, &header);
    
    header.type = type;
    kmalloc_header_write(header_address, &header);
}

uint8_t kmalloc_get_flags(uint32_t address) {
    uint32_t             header_address;
    struct KMallocHeader header;
    
    if (!kmalloc_is_valid(address))
        return 0;
    
    header_address = address - KMALLOC_HEADER_SIZE;
    kmalloc_header_read(header_address, &header);
    
    return header.flags;
}

void kmalloc_set_flags(uint32_t address, uint8_t flags) {
    uint32_t             header_address;
    struct KMallocHeader header;
    
    if (!kmalloc_is_valid(address))
        return;
    
    header_address = address - KMALLOC_HEADER_SIZE;
    kmalloc_header_read(header_address, &header);
    
    header.flags = flags;
    kmalloc_header_write(header_address, &header);
}

uint8_t kmalloc_get_permissions(uint32_t address) {
    uint32_t             header_address;
    struct KMallocHeader header;
    
    if (!kmalloc_is_valid(address))
        return 0;
    
    header_address = address - KMALLOC_HEADER_SIZE;
    kmalloc_header_read(header_address, &header);
    
    return header.perm;
}

void kmalloc_set_permissions(uint32_t address, uint8_t permissions) {
    uint32_t             header_address;
    struct KMallocHeader header;
    
    if (!kmalloc_is_valid(address))
        return;
    
    header_address = address - KMALLOC_HEADER_SIZE;
    kmalloc_header_read(header_address, &header);
    
    header.perm = permissions;
    kmalloc_header_write(header_address, &header);
}

uint32_t kmalloc_get_size(uint32_t address) {
    uint32_t             header_address;
    struct KMallocHeader header;
    
    if (!kmalloc_is_valid(address))
        return 0;
    
    header_address = address - KMALLOC_HEADER_SIZE;
    kmalloc_header_read(header_address, &header);
    
    return header.size;
}

uint32_t kmalloc_next(uint32_t previous_address) {
    uint32_t             start_block;
    uint32_t             current_block;
    uint32_t             header_address;
    uint32_t             total_size;
    uint32_t             used_blocks;
    struct KMallocHeader header;
    
    if (previous_address == KMALLOC_NULL) {
        start_block = kmalloc_reserved_blocks;
    } else {
        if (!kmalloc_is_valid(previous_address))
            return KMALLOC_NULL;
        
        header_address = previous_address - KMALLOC_HEADER_SIZE;
        kmalloc_header_read(header_address, &header);
        
        total_size  = header.size + KMALLOC_HEADER_SIZE;
        used_blocks = kmalloc_blocks_required(total_size);
        
        start_block = (header_address / kmalloc_block_size) + used_blocks;
    }
    
    for (current_block = start_block; current_block < kmalloc_block_count; current_block++) {
        if (!kmalloc_block_is_used(current_block))
            continue;
        
        header_address = current_block * kmalloc_block_size;
        
        if (header_address < kmalloc_heap_data_begin())
            continue;
        
        kmalloc_header_read(header_address, &header);
        
        if (header.magic != KMALLOC_MAGIC)
            continue;
        
        if (header.size == 0)
            continue;
        
        if ((header_address + KMALLOC_HEADER_SIZE) >= kmalloc_pool_size)
            continue;
        
        return header_address + KMALLOC_HEADER_SIZE;
    }
    
    return KMALLOC_NULL;
}

void kmem_write(uint32_t address, const void* source, uint32_t size) {
    const uint8_t* bytes = (const uint8_t*)source;
    uint32_t       index;
    
    for (index = 0; index < size; index++) {
        kmem_write8(address + index, bytes[index]);
    }
}

void kmem_read(uint32_t address, void* destination, uint32_t size) {
    uint8_t* bytes = (uint8_t*)destination;
    uint32_t index;
    
    for (index = 0; index < size; index++) {
        bytes[index] = kmem_read8(address + index);
    }
}

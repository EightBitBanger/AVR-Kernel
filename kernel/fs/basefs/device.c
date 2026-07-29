#include <kernel/fs/fs.h>
#include <kernel/util/string.h>

uint8_t fs_device_get_partition(struct FSDeviceContext* device_context, struct FSPartitionBlock* part) {
    if (!device_context) return 1;
    uint32_t offset = sizeof(struct FSDeviceHeader);
    fs_mem_read(device_context, offset, part, sizeof(struct FSPartitionBlock));
    if (part->magic != FS_MAGIC) return 1;
    return 0;
}

struct FSDeviceContext fs_device_open(uint32_t device_address, struct FSPartitionBlock* partition, uint16_t device_type) {
    struct FSDeviceContext new_context;
    memset(&new_context, 0x00, sizeof(struct FSDeviceContext));
    
    new_context.device_address = device_address;
    new_context.device_type    = device_type;
    new_context.frame_offset   = 0xFFFFFFFF;
    new_context.sector_frame   = 0xFFFFFFFF;
    
    if (fs_device_get_partition(&new_context, partition) != 0) 
        return new_context;
    
    new_context.is_open          = true;
    new_context.sector_size      = partition->sector_size;
    new_context.pool_size        = partition->total_size;
    new_context.block_count      = new_context.pool_size / new_context.sector_size;
    new_context.bitmap_size      = (new_context.block_count + 7UL) / 8UL;
    uint32_t meta_sz = sizeof(struct FSDeviceHeader) + sizeof(struct FSPartitionBlock) + new_context.bitmap_size;
    new_context.reserved_blocks  = (meta_sz + new_context.sector_size - 1UL) / new_context.sector_size;
    new_context.frame_dirty      = false;
    new_context.sector_dirty     = false;
    
    return new_context;
}

uint32_t fs_get_used_bytes(struct FSDeviceContext* device_context) {
    if (!device_context || device_context->pool_size == 0) return 0; 
    
    uint32_t used_bytes = 0;
    uint32_t reserved_bytes = device_context->reserved_blocks * device_context->sector_size;
    
    // Guard against corrupted reserved block counts
    if (reserved_bytes > device_context->pool_size) {
        return device_context->pool_size;
    }
    used_bytes += reserved_bytes;
    
    uint32_t current_alloc = fs_find_next(device_context, FS_NULL); 
    while (current_alloc != FS_NULL) {
        uint32_t header_addr = current_alloc - sizeof(struct FSAllocHeader); 
        struct FSAllocHeader header;
        fs_mem_read(device_context, header_addr, &header, sizeof(struct FSAllocHeader)); 
        
        // Sanity check to prevent corrupted headers from overflowing calculation
        if (header.size == 0 || header.size > device_context->pool_size) {
            break;
        }
        
        uint32_t blocks_used = (sizeof(struct FSAllocHeader) + header.size + device_context->sector_size - 1UL) / device_context->sector_size; 
        uint32_t block_bytes = blocks_used * device_context->sector_size;
        
        // Prevent overflow beyond total pool size
        if (used_bytes + block_bytes > device_context->pool_size) {
            used_bytes = device_context->pool_size;
            break;
        }
        
        used_bytes += block_bytes; 
        current_alloc = fs_find_next(device_context, current_alloc); 
    }
    
    return used_bytes;
}

uint32_t fs_alloc(struct FSDeviceContext* device_context, uint32_t size) {
    if (!device_context) return FS_NULL;
    struct FSAllocHeader header;
    uint32_t blocks_needed = (sizeof(struct FSAllocHeader) + size + device_context->sector_size - 1UL) / device_context->sector_size;
    uint32_t run_start = 0, run_length = 0;
    
    for (uint32_t i = device_context->reserved_blocks; i < device_context->block_count; i++) {
        if (!fs_bitmap_get(device_context, i)) {
            if (run_length == 0) run_start = i;
            if (++run_length >= blocks_needed) {
                for (uint32_t j = run_start; j < run_start + blocks_needed; j++) {
                    fs_bitmap_set(device_context, j);
                }
                fs_bitmap_flush(device_context);
                
                uint32_t addr = run_start * device_context->sector_size;
                header.size = size;
                fs_mem_write(device_context, addr, &header, sizeof(struct FSAllocHeader));
                return addr + sizeof(struct FSAllocHeader);
            }
        } else {
            run_length = 0;
        }
    }
    return FS_NULL;
}

void fs_free(struct FSDeviceContext* device_context, uint32_t address) {
    if (!device_context || address == FS_NULL) return;
    uint32_t alloc_addr = address - sizeof(struct FSAllocHeader);
    struct FSAllocHeader header;
    fs_mem_read(device_context, alloc_addr, &header, sizeof(struct FSAllocHeader));
    
    uint32_t blocks = (sizeof(struct FSAllocHeader) + header.size + device_context->sector_size - 1UL) / device_context->sector_size;
    uint32_t start_block = alloc_addr / device_context->sector_size;
    
    for (uint32_t i = 0; i < blocks; i++) {
        fs_bitmap_clear(device_context, start_block + i);
    }
    
    header.size = 0;
    fs_mem_write(device_context, alloc_addr, &header, sizeof(struct FSAllocHeader));
    fs_bitmap_flush(device_context);
}

uint32_t fs_find_next(struct FSDeviceContext* device_context, uint32_t prev_addr) {
    if (!device_context) return FS_NULL;
    uint32_t start_block;
    struct FSAllocHeader header;
    
    if (prev_addr == FS_NULL) {
        start_block = device_context->reserved_blocks;
    } else {
        uint32_t h_addr = prev_addr - sizeof(struct FSAllocHeader);
        fs_mem_read(device_context, h_addr, &header, sizeof(struct FSAllocHeader));
        uint32_t used = (header.size + sizeof(struct FSAllocHeader) + device_context->sector_size - 1UL) / device_context->sector_size;
        start_block = (h_addr / device_context->sector_size) + used;
    }
    
    for (uint32_t i = start_block; i < device_context->block_count; i++) {
        if (fs_bitmap_get(device_context, i)) {
            uint32_t addr = i * device_context->sector_size;
            fs_mem_read(device_context, addr, &header, sizeof(struct FSAllocHeader));
            if (header.size != 0) return addr + sizeof(struct FSAllocHeader);
        }
    }
    return FS_NULL;
}

void fs_device_format(uint32_t device_address, uint32_t capacity, uint32_t sector_size, uint16_t device_type) {
    struct FSDeviceHeader devH = { .id = 0x13, .name = "fs" };
    struct FSPartitionBlock partH = {
        .total_size = capacity, 
        .sector_size = sector_size, 
        .magic = FS_MAGIC, 
        .name = "storage"
    };
    struct FSPartitionBlock part;
    struct FSDeviceContext ctx = fs_device_open(device_address, &part, device_type);
    
    ctx.device_address = device_address;
    uint32_t b_size = ((capacity / sector_size) + 7) / 8;
    uint32_t meta_size = sizeof(devH) + sizeof(partH) + b_size;
    uint32_t res_blocks = (meta_size + sector_size - 1) / sector_size;
    
    fs_mem_write(&ctx, 0, &devH, sizeof(devH));
    fs_mem_write(&ctx, sizeof(devH), &partH, sizeof(partH));
    
    uint32_t b_addr = sizeof(devH) + sizeof(partH);
    for (uint32_t i = 0; i < b_size; i++) fs_writeb(&ctx, b_addr + i, 0x00);
    
    ctx = fs_device_open(device_address, &partH, device_type);
    for (uint32_t i = 0; i < res_blocks; i++) fs_bitmap_set(&ctx, i);
    fs_bitmap_flush(&ctx);
}

void fs_device_format_low(uint32_t device_address, uint32_t capacity) {
    uint8_t clear_byte = 0x00;
    struct FSPartitionBlock part;
    struct FSDeviceContext ctx = fs_device_open(device_address, &part, FS_DEVICE_TYPE_ATA);
    
    for (uint64_t address_range = 0; address_range < capacity; address_range++) 
        fs_writeb(&ctx, (uint32_t)address_range, clear_byte);
    fs_cache_sync(&ctx);
}

bool fs_check_directory_valid(struct FSDeviceContext* ctx, uint32_t address) {
    if (!ctx) return false;
    struct FSFileHeader header;
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    if (header.block.attributes & FS_ATTRIBUTE_DIRECTORY) 
        return true;
    return false;
}

#include <kernel/fs/fs.h>

#include <string.h>

uint32_t fs_device_address  = 0;
uint32_t fs_sector_size     = 0;
uint32_t fs_pool_size       = 0;
uint32_t fs_block_count     = 0;
uint32_t fs_bitmap_size     = 0;
uint32_t fs_reserved_blocks = 0;

uint8_t  fs_frame_bitmap[BITMAP_FRAME_SIZE];
uint32_t fs_frame_offset = FS_INVALID_FRAME;
bool     fs_frame_dirty = false;

struct Bus fs_bus;

void fs_init(void) {
    fs_bus.write_waitstate = 1;
    fs_bus.read_waitstate  = 2;
}


uint8_t fs_device_get_partition(uint32_t device_address, struct FSPartitionBlock* part) {
    fs_device_address = device_address;
    fs_mem_read(sizeof(struct FSDeviceHeader), part, sizeof(struct FSPartitionBlock));
    if (part->magic != FS_MAGIC) return 1;
    return 0;
}

uint8_t fs_device_open(uint32_t device_address, struct FSPartitionBlock* partition) {
    struct FSDeviceHeader deviceHeader;
    fs_device_address = device_address;
    
    fs_mem_read(0, &deviceHeader, sizeof(struct FSDeviceHeader));
    if (fs_device_get_partition(device_address, partition) != 0) return 1;
    
    fs_sector_size = partition->sector_size;
    fs_pool_size   = partition->total_size;
    fs_block_count = fs_pool_size / fs_sector_size;
    fs_bitmap_size = (fs_block_count + 7UL) / 8UL;
    
    uint32_t metadata_size = sizeof(struct FSDeviceHeader) + sizeof(struct FSPartitionBlock) + fs_bitmap_size;
    fs_reserved_blocks = (metadata_size + fs_sector_size - 1UL) / fs_sector_size;
    
    // Invalidate window to force load on first access
    fs_frame_offset = 0xFFFFFFFF;
    fs_frame_dirty = false;
    
    return 0;
}


uint32_t fs_alloc(uint32_t size) {
    struct FSAllocHeader header;
    uint32_t blocks_needed = (sizeof(struct FSAllocHeader) + size + fs_sector_size - 1UL) / fs_sector_size;
    uint32_t run_start = 0, run_length = 0;
    
    for (uint32_t i = fs_reserved_blocks; i < fs_block_count; i++) {
        if (!fs_bitmap_get(i)) {
            if (run_length == 0) run_start = i;
            if (++run_length >= blocks_needed) {
                for (uint32_t j = run_start; j < run_start + blocks_needed; j++) {
                    fs_bitmap_set(j);
                }
                fs_bitmap_flush(); // Important to commit allocation
                
                uint32_t addr = run_start * fs_sector_size;
                header.size = size;
                fs_mem_write(addr, &header, sizeof(struct FSAllocHeader));
                return addr + sizeof(struct FSAllocHeader);
            }
        } else {
            run_length = 0;
        }
    }
    return FS_NULL;
}

void fs_free(uint32_t address) {
    if (address == FS_NULL) return;
    uint32_t alloc_addr = address - sizeof(struct FSAllocHeader);
    struct FSAllocHeader header;
    fs_mem_read(alloc_addr, &header, sizeof(struct FSAllocHeader));
    
    uint32_t blocks = (sizeof(struct FSAllocHeader) + header.size + fs_sector_size - 1UL) / fs_sector_size;
    uint32_t start_block = alloc_addr / fs_sector_size;
    
    for (uint32_t i = 0; i < blocks; i++) {
        fs_bitmap_clear(start_block + i);
    }
    
    header.size = 0;
    fs_mem_write(alloc_addr, &header, sizeof(struct FSAllocHeader));
    fs_bitmap_flush();
}

uint32_t fs_find_next(uint32_t prev_addr) {
    uint32_t start_block;
    struct FSAllocHeader header;
    
    if (prev_addr == FS_NULL) {
        start_block = fs_reserved_blocks;
    } else {
        uint32_t h_addr = prev_addr - sizeof(struct FSAllocHeader);
        fs_mem_read(h_addr, &header, sizeof(struct FSAllocHeader));
        uint32_t used = (header.size + sizeof(struct FSAllocHeader) + fs_sector_size - 1UL) / fs_sector_size;
        start_block = (h_addr / fs_sector_size) + used;
    }
    
    for (uint32_t i = start_block; i < fs_block_count; i++) {
        if (fs_bitmap_get(i)) {
            uint32_t addr = i * fs_sector_size;
            fs_mem_read(addr, &header, sizeof(struct FSAllocHeader));
            if (header.size != 0) return addr + sizeof(struct FSAllocHeader);
        }
    }
    return FS_NULL;
}

void fs_device_format(uint32_t device_address, uint32_t capacity, uint32_t sector_size) {
    struct FSDeviceHeader devH = { .id = 0x13, .name = "fs" };
    struct FSPartitionBlock partH = {
        .total_size = capacity, .sector_size = sector_size, 
        .magic = FS_MAGIC, .name = "eeprom"
    };
    
    fs_device_address = device_address;
    uint32_t b_size = ((capacity / sector_size) + 7) / 8;
    uint32_t meta_size = sizeof(devH) + sizeof(partH) + b_size;
    uint32_t res_blocks = (meta_size + sector_size - 1) / sector_size;

    fs_mem_write(0, &devH, sizeof(devH));
    fs_mem_write(sizeof(devH), &partH, sizeof(partH));

    // Clear bitmap area on device
    uint32_t b_addr = sizeof(devH) + sizeof(partH);
    for (uint32_t i = 0; i < b_size; i++) fs_writeb(b_addr + i, 0x00);

    // Re-open to initialize window logic
    fs_device_open(device_address, &partH);

    // Mark reserved blocks
    for (uint32_t i = 0; i < res_blocks; i++) fs_bitmap_set(i);
    fs_bitmap_flush();
}

bool fs_check_directory_valid(uint32_t address) {
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    if (header.block.attributes & FS_ATTRIBUTE_DIRECTORY) 
        return true;
    return false;
}

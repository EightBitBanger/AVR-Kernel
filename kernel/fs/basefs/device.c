#include <kernel/fs/fs.h>
#include <kernel/bus/bus.h>

#include <kernel/arch/avr/io.h>
#include <kernel/boot/avr/heap.h>

#include <kernel/delay.h>

#include <string.h>

static uint32_t fs_device_address = 0;

static uint32_t fs_sector_size    = 0;
static uint32_t fs_pool_size      = 0;
static uint32_t fs_block_count    = 0;

static uint32_t fs_bitmap_size     = 0;
static uint32_t fs_reserved_blocks = 0;

static uint8_t* fs_bitmap = 0;
static uint8_t* fs_bitmap_dirty = 0;

static struct Bus fs_bus;

void fs_init(void) {
    fs_bus.write_waitstate = 1;
    fs_bus.read_waitstate  = 2;
}

static inline uint32_t fs_heap_data_begin(void) {
    return fs_reserved_blocks * fs_sector_size;
}

static inline uint32_t fs_heap_data_size(void) {
    if (fs_pool_size <= fs_heap_data_begin())
        return 0;
    return fs_pool_size - fs_heap_data_begin();
}

static inline uint32_t fs_bitmap_address(void) {
    return sizeof(struct FSDeviceHeader) + sizeof(struct FSPartitionBlock);
}

static inline void fs_bitmap_mark_dirty(uint32_t byte_index) {
    if (fs_bitmap_dirty != NULL) {
        fs_bitmap_dirty[byte_index] = 1;
    }
}

static inline void fs_bitmap_set(uint32_t index) {
    uint32_t byte_index = index >> 3;
    uint8_t  mask       = (uint8_t)(1U << (index & 7U));
    uint8_t  old_value;
    uint8_t  new_value;
    
    old_value = fs_bitmap[byte_index];
    new_value = (uint8_t)(old_value | mask);

    if (new_value != old_value) {
        fs_bitmap[byte_index] = new_value;
        fs_bitmap_mark_dirty(byte_index);
    }
}

static inline void fs_bitmap_clear(uint32_t index) {
    uint32_t byte_index = index >> 3;
    uint8_t  mask       = (uint8_t)(1U << (index & 7U));
    uint8_t  old_value;
    uint8_t  new_value;
    
    old_value = fs_bitmap[byte_index];
    new_value = (uint8_t)(old_value & (uint8_t)~mask);
    
    if (new_value != old_value) {
        fs_bitmap[byte_index] = new_value;
        fs_bitmap_mark_dirty(byte_index);
    }
}

static inline bool fs_bitmap_get(uint32_t index) {
    return (fs_bitmap[index >> 3] & (uint8_t)(1U << (index & 7U))) != 0;
}

static inline void fs_writeb(uint32_t address, uint8_t byte) {
    uint8_t check_byte;
    mmio_readb(&fs_bus, fs_device_address + address, &check_byte);
    if (byte == check_byte) 
        return;
    mmio_writeb(&fs_bus, fs_device_address + address, &byte);
    check_byte = ~byte;
    uint16_t retry = 1000;
    while (check_byte != byte && retry != 0) {
        _delay_us(100);
        mmio_readb(&fs_bus, fs_device_address + address, &check_byte);
        retry--;
    }
}

static inline uint8_t fs_readb(uint32_t address) {
    uint8_t byte;
    mmio_readb(&fs_bus, fs_device_address + address, &byte);
    return byte;
}

static void fs_write32(uint32_t address, uint32_t value) {
    fs_writeb(address + 0, (uint8_t)((value >> 0)  & 0xFF));
    fs_writeb(address + 1, (uint8_t)((value >> 8)  & 0xFF));
    fs_writeb(address + 2, (uint8_t)((value >> 16) & 0xFF));
    fs_writeb(address + 3, (uint8_t)((value >> 24) & 0xFF));
}

static uint32_t fs_read32(uint32_t address) {
    uint32_t value = 0;

    value |= ((uint32_t)fs_readb(address + 0) << 0);
    value |= ((uint32_t)fs_readb(address + 1) << 8);
    value |= ((uint32_t)fs_readb(address + 2) << 16);
    value |= ((uint32_t)fs_readb(address + 3) << 24);

    return value;
}

void fs_mem_write(uint32_t address, const void* source, uint32_t size) {
    const uint8_t* bytes = (const uint8_t*)source;
    
    for (uint32_t index = 0; index < size; index++) 
        fs_writeb(address + index, bytes[index]);
}

void fs_mem_read(uint32_t address, void* destination, uint32_t size) {
    uint8_t* bytes = (uint8_t*)destination;
    
    for (uint32_t index = 0; index < size; index++) 
        bytes[index] = fs_readb(address + index);
}

bool fs_device_check(uint32_t address, uint32_t size) {
    if (address == FS_NULL)
        return false;
    
    if (size > fs_pool_size)
        return false;
    
    if (address > (fs_pool_size - size))
        return false;
    
    return true;
}

uint8_t fs_device_get_partition_header(uint32_t device_address, struct FSPartitionBlock* part) {
    struct FSPartitionBlock part_block;
    
    fs_device_address = device_address;
    
    fs_mem_read(sizeof(struct FSDeviceHeader), &part_block, sizeof(struct FSPartitionBlock));
    
    if (part_block.magic != FS_MAGIC) 
        return 1;
    
    if (part_block.sector_size == 0) 
        return 2;
    
    if (part_block.total_size == 0) 
        return 3;
    
    memcpy(part, &part_block, sizeof(struct FSPartitionBlock));
    
    return 0;
}

uint8_t fs_device_open(uint32_t device_address, uint8_t* bitmap, uint8_t* bitmap_dirty, struct FSPartitionBlock* partition) {
    struct FSDeviceHeader    deviceHeader;
    struct FSPartitionBlock  partition_block;
    uint32_t                 metadata_size;
    
    fs_device_address = device_address;
    
    fs_mem_read(0x00000, &deviceHeader, sizeof(struct FSDeviceHeader));
    fs_mem_read(sizeof(struct FSDeviceHeader), &partition_block, sizeof(struct FSPartitionBlock));
    
    fs_device_get_partition_header(device_address, &partition_block);
    
    fs_sector_size = partition_block.sector_size;
    fs_pool_size   = partition_block.total_size;
    fs_block_count = fs_pool_size / fs_sector_size;
    
    if (fs_block_count == 0)
        return 4;
    
    fs_bitmap_size = (fs_block_count + 7UL) / 8UL;
    
    metadata_size = sizeof(struct FSDeviceHeader)
                + sizeof(struct FSPartitionBlock)
                + fs_bitmap_size;
    
    fs_reserved_blocks = (metadata_size + fs_sector_size - 1UL) / fs_sector_size;
    
    if (bitmap != NULL) {
        fs_mem_read(fs_bitmap_address(), bitmap, fs_bitmap_size);
    }
    
    fs_bitmap = bitmap;
    fs_bitmap_dirty = bitmap_dirty;
    
    if (fs_bitmap_dirty != NULL) {
        memset(fs_bitmap_dirty, 0x00, fs_bitmap_size);
    }
    
    *partition = partition_block;
    
    return 0;
}

void fs_device_format(uint32_t device_address, uint32_t capacity_max, uint32_t sector_size) {
    struct FSDeviceHeader    deviceHeader;
    struct FSPartitionBlock  headerBlock;
    uint32_t                 block_count;
    uint32_t                 bitmap_size;
    uint32_t                 metadata_size;
    uint32_t                 reserved_blocks;
    uint32_t                 index;
    
    if (sector_size == 0)
        return;
    
    block_count = capacity_max / sector_size;
    
    if (block_count == 0)
        return;
    
    fs_device_address = device_address;
    
    bitmap_size = (block_count + 7UL) / 8UL;
    
    metadata_size = sizeof(struct FSDeviceHeader)
                + sizeof(struct FSPartitionBlock)
                + bitmap_size;
    
    reserved_blocks = (metadata_size + sector_size - 1UL) / sector_size;
    
    memset(&deviceHeader, 0x00, sizeof(struct FSDeviceHeader));
    strcpy(deviceHeader.name, "fs");
    deviceHeader.id = 0x13;
    
    memset(&headerBlock, 0x00, sizeof(struct FSPartitionBlock));
    headerBlock.total_size     = capacity_max;
    headerBlock.sector_size    = sector_size;
    headerBlock.root_directory = FS_NULL;
    strcpy(headerBlock.name, "eeprom");
    headerBlock.flags = 0;
    headerBlock.type  = 0;
    headerBlock.magic = FS_MAGIC;
    
    fs_mem_write(0x00000, &deviceHeader, sizeof(struct FSDeviceHeader));
    fs_mem_write(sizeof(struct FSDeviceHeader), &headerBlock, sizeof(struct FSPartitionBlock));
    
    for (index = 0; index < bitmap_size; index++) {
        fs_writeb(fs_bitmap_address() + index, 0x00);
    }
    
    for (index = 0; index < reserved_blocks; index++) {
        uint32_t byte_index = index >> 3;
        uint8_t  byte_value = fs_readb(fs_bitmap_address() + byte_index);
        
        byte_value |= (uint8_t)(1U << (index & 7U));
        
        fs_writeb(fs_bitmap_address() + byte_index, byte_value);
    }
    
    fs_sector_size     = sector_size;
    fs_pool_size       = capacity_max;
    fs_block_count     = block_count;
    fs_bitmap_size     = bitmap_size;
    fs_reserved_blocks = reserved_blocks;
}


uint32_t fs_find_next(uint32_t previous_address) {
    uint32_t             start_block;
    uint32_t             current_block;
    uint32_t             header_address;
    uint32_t             total_size;
    uint32_t             used_blocks;
    struct FSAllocHeader header;
    
    if (fs_bitmap == NULL)
        return FS_NULL;
    
    if (fs_sector_size == 0)
        return FS_NULL;
    
    if (previous_address == FS_NULL) {
        start_block = fs_reserved_blocks;
    } else {
        if (previous_address < sizeof(struct FSAllocHeader))
            return FS_NULL;
        
        header_address = previous_address - sizeof(struct FSAllocHeader);
        
        if (header_address >= fs_pool_size)
            return FS_NULL;
        
        if ((header_address % fs_sector_size) != 0)
            return FS_NULL;
        
        fs_mem_read(header_address, &header, sizeof(struct FSAllocHeader));
        
        if (header.size == 0)
            return FS_NULL;
        
        total_size = header.size + sizeof(struct FSAllocHeader);
        used_blocks = (total_size + fs_sector_size - 1UL) / fs_sector_size;
        
        if (used_blocks == 0)
            return FS_NULL;
        
        start_block = (header_address / fs_sector_size) + used_blocks;
    }
    
    for (current_block = start_block; current_block < fs_block_count; current_block++) {
        if (!fs_bitmap_get(current_block))
            continue;
        
        header_address = current_block * fs_sector_size;
        
        if (header_address < fs_heap_data_begin())
            continue;
        
        if ((header_address + sizeof(struct FSAllocHeader)) > fs_pool_size)
            continue;
        
        fs_mem_read(header_address, &header, sizeof(struct FSAllocHeader));
        
        if (header.size == 0)
            continue;
        
        if ((header_address + sizeof(struct FSAllocHeader) + header.size) > fs_pool_size)
            continue;
        
        return header_address + sizeof(struct FSAllocHeader);
    }
    
    return FS_NULL;
}

uint32_t fs_alloc(uint32_t size) {
    struct FSAllocHeader header;
    uint32_t             total_size;
    uint32_t             blocks_needed;
    uint32_t             block_index;
    uint32_t             run_start;
    uint32_t             run_length;
    uint32_t             mark_index;
    uint32_t             allocation_address;
    
    if (fs_bitmap == NULL)
        return FS_NULL;
    
    if (size == 0)
        return FS_NULL;
    
    if (fs_sector_size == 0)
        return FS_NULL;
    
    if (fs_block_count <= fs_reserved_blocks)
        return FS_NULL;
    
    total_size = sizeof(struct FSAllocHeader) + size;
    blocks_needed = (total_size + fs_sector_size - 1UL) / fs_sector_size;
    
    if (blocks_needed == 0)
        return FS_NULL;
    
    run_start  = FS_NULL;
    run_length = 0;
    
    for (block_index = fs_reserved_blocks; block_index < fs_block_count; block_index++) {
        if (!fs_bitmap_get(block_index)) {
            if (run_length == 0) {
                run_start = block_index;
            }
            
            run_length++;
            
            if (run_length >= blocks_needed) {
                for (mark_index = run_start; mark_index < (run_start + blocks_needed); mark_index++) {
                    fs_bitmap_set(mark_index);
                }
                
                allocation_address = run_start * fs_sector_size;
                
                header.size = size;
                fs_mem_write(allocation_address, &header, sizeof(struct FSAllocHeader));
                
                return allocation_address + sizeof(struct FSAllocHeader);
            }
        } else {
            run_start  = FS_NULL;
            run_length = 0;
        }
    }
    
    return FS_NULL;
}

void fs_free(uint32_t address) {
    struct FSAllocHeader header;
    uint32_t             allocation_address;
    uint32_t             total_size;
    uint32_t             start_block;
    uint32_t             blocks_to_free;
    uint32_t             block_index;
    
    if (fs_bitmap == NULL)
        return;
    
    if (address == FS_NULL)
        return;
    
    if (fs_sector_size == 0)
        return;
    
    if (address < sizeof(struct FSAllocHeader))
        return;
    
    allocation_address = address - sizeof(struct FSAllocHeader);
    
    if (allocation_address >= fs_pool_size)
        return;
    
    fs_mem_read(allocation_address, &header, sizeof(struct FSAllocHeader));
    
    if (header.size == 0)
        return;
    
    total_size = sizeof(struct FSAllocHeader) + header.size;
    blocks_to_free = (total_size + fs_sector_size - 1UL) / fs_sector_size;
    
    start_block = allocation_address / fs_sector_size;
    
    if (start_block < fs_reserved_blocks)
        return;
    
    if (start_block >= fs_block_count)
        return;
    
    for (block_index = 0; block_index < blocks_to_free; block_index++) {
        uint32_t current_block = start_block + block_index;
        
        if (current_block >= fs_block_count)
            break;
        
        if (current_block < fs_reserved_blocks)
            continue;
        
        fs_bitmap_clear(current_block);
    }
    
    header.size = 0;
    fs_mem_write(allocation_address, &header, sizeof(struct FSAllocHeader));
}

void fs_bitmap_flush(void) {
    uint32_t index;
    
    if (fs_bitmap == NULL)
        return;
    
    if (fs_bitmap_size == 0)
        return;
    
    if (fs_bitmap_dirty == NULL) {
        fs_mem_write(fs_bitmap_address(), fs_bitmap, fs_bitmap_size);
        return;
    }
    
    for (index = 0; index < fs_bitmap_size; index++) {
        if (fs_bitmap_dirty[index] == 0)
            continue;
        
        fs_writeb(fs_bitmap_address() + index, fs_bitmap[index]);
        fs_bitmap_dirty[index] = 0;
    }
}


bool fs_is_directory(uint32_t address) {
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    if (header.block.attributes & FS_ATTRIBUTE_DIRECTORY) 
        return true;
    return false;
}

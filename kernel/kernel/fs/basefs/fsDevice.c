#include <kernel/fs/fs.h>
#include <kernel/bus/bus.h>

#include <kernel/arch/avr/io.h>
#include <kernel/boot/avr/heap.h>

#include <kernel/delay.h>

#include <string.h>
#include <stdbool.h>

const uint8_t DeviceHeaderString[] = {0x13, 'f','s',' ',' ',' ',' ',' ',' ',' ',' '};

struct Bus fs_bus;

static uint32_t fs_device_address = 0;

uint8_t* fs_bitmap = 0;

static uint32_t fs_sector_size      = 0;
static uint32_t fs_pool_size        = 0;
static uint32_t fs_block_count      = 0;

static uint32_t fs_bitmap_size       = 0;
static uint32_t fs_reserved_blocks   = 0;


static inline uint32_t fs_heap_data_begin(void) {
    return fs_reserved_blocks * fs_sector_size;
}

static inline uint32_t fs_heap_data_size(void) {
    if (fs_pool_size <= fs_heap_data_begin())
        return 0;
    return fs_pool_size - fs_heap_data_begin();
}

void fs_init(void) {
    
    fs_bus.write_waitstate = 1;
    fs_bus.read_waitstate  = 2;
}

void fs_writeb(uint32_t address, uint8_t byte) {
    mmio_writeb(&fs_bus, fs_device_address + address, &byte);
    //_delay_ms(5);
}

void fs_readb(uint32_t address, uint8_t* byte) {
    mmio_readb(&fs_bus, fs_device_address + address, byte);
}

static inline void fs_write8(uint32_t address, uint8_t value) {
    fs_writeb(address, value);
}

static inline uint8_t fs_read8(uint32_t address) {
    uint8_t byte;
    fs_readb(address, &byte);
    return byte;
}

static void fs_write32(uint32_t address, uint32_t value) {
    fs_write8(address + 0, (uint8_t)((value >> 0)  & 0xFF));
    fs_write8(address + 1, (uint8_t)((value >> 8)  & 0xFF));
    fs_write8(address + 2, (uint8_t)((value >> 16) & 0xFF));
    fs_write8(address + 3, (uint8_t)((value >> 24) & 0xFF));
}

static uint32_t fs_read32(uint32_t address) {
    uint32_t value = 0;
    
    value |= ((uint32_t)fs_read8(address + 0) << 0);
    value |= ((uint32_t)fs_read8(address + 1) << 8);
    value |= ((uint32_t)fs_read8(address + 2) << 16);
    value |= ((uint32_t)fs_read8(address + 3) << 24);

    return value;
}

static void fs_header_write(uint32_t address, const struct FSHeaderBlock* header) {
    fs_write32(address + 0, header->size);
    fs_write8 (address + 4, header->flags);
    fs_write8 (address + 5, header->type);
    fs_write8 (address + 6, header->reserved);
    fs_write8 (address + 7, header->magic);
}

static void fs_header_read(uint32_t address, struct FSHeaderBlock* header) {
    header->size     = fs_read32(address + 0);
    header->flags    = fs_read8(address + 4);
    header->type     = fs_read8(address + 5);
    header->reserved = fs_read8(address + 6);
    header->magic    = fs_read8(address + 7);
}

static inline uint32_t fs_blocks_required(uint32_t size) {
    if (size == 0)
        return 0;
    return (size + (fs_sector_size - 1UL)) / fs_sector_size;
}

static inline bool fs_block_is_used(uint32_t block_index) {
    uint32_t byte_index = block_index / 8UL;
    uint8_t  bit_index  = (uint8_t)(block_index % 8UL);
    return (fs_bitmap[byte_index] & (1U << bit_index)) != 0;
}

static inline void fs_block_set_used(uint32_t block_index) {
    uint32_t byte_index = block_index / 8UL;
    uint8_t  bit_index  = (uint8_t)(block_index % 8UL);
    fs_bitmap[byte_index] |= (uint8_t)(1U << bit_index);
}

static inline void fs_block_set_free(uint32_t block_index) {
    uint32_t byte_index = block_index / 8UL;
    uint8_t  bit_index  = (uint8_t)(block_index % 8UL);
    fs_bitmap[byte_index] &= (uint8_t)~(1U << bit_index);
}

bool fs_is_valid(uint32_t address) {
    uint32_t header_address;
    uint32_t total_size;
    uint32_t required_blocks;
    uint32_t start_block;
    struct FSHeaderBlock header;
    
    if (address == FS_NULL)
        return false;
    
    if (address < (fs_heap_data_begin() + FS_HEADER_SIZE))
        return false;
    
    if (address >= fs_pool_size)
        return false;
    
    header_address = address - FS_HEADER_SIZE;
    
    if (header_address < fs_heap_data_begin())
        return false;
    
    if (header_address >= fs_pool_size)
        return false;
    
    fs_header_read(header_address, &header);
    
    if (header.magic != FS_MAGIC)
        return false;
    
    if (header.size == 0)
        return false;
    
    total_size      = header.size + FS_HEADER_SIZE;
    required_blocks = fs_blocks_required(total_size);
    start_block     = header_address / fs_sector_size;
    
    if (required_blocks == 0)
        return false;
    
    if (start_block < fs_reserved_blocks)
        return false;
    
    if ((start_block + required_blocks) > fs_block_count)
        return false;
    
    return true;
}

void fs_bitmap_write(void) {
    if (fs_bitmap == 0)
        return;
    
    for (uint32_t address = 0; address < fs_bitmap_size; address++) {
        fs_writeb(address, fs_bitmap[address]);
    }
}

void fs_bitmap_read(void) {
    if (fs_bitmap == 0)
        return;
    
    for (uint32_t address = 0; address < fs_bitmap_size; address++) {
        fs_readb(address, &fs_bitmap[address]);
    }
}


uint32_t fs_device_open(uint32_t device_address, uint8_t* bitmap, uint32_t sector_size, uint32_t total_size) {
    fs_device_address = device_address;
    fs_bitmap = bitmap;
    if (sector_size == 0 || total_size == 0) {
        fs_sector_size      = 0;
        fs_pool_size       = 0;
        fs_block_count     = 0;
        fs_bitmap_size     = 0;
        fs_reserved_blocks = 0;
        return 0;
    }
    
    fs_sector_size  = sector_size;
    fs_pool_size    = total_size;
    fs_block_count  = fs_pool_size / fs_sector_size;
    
    fs_bitmap_size     = (fs_block_count + 7UL) / 8UL;
    fs_reserved_blocks = (fs_bitmap_size + fs_sector_size - 1UL) / fs_sector_size;
    
    if (fs_bitmap == 0)
        return 0;
    
    for (uint32_t index = 0; index < fs_bitmap_size; index++) {
        fs_bitmap[index] = 0;
    }
    fs_bitmap_read();
    
    return fs_device_address;
}




uint32_t fs_alloc(uint32_t size) {
    uint32_t total_size;
    uint32_t required_blocks;
    uint32_t start_block;
    uint32_t current_block;
    uint32_t found_blocks;
    
    if (size == 0)
        return FS_NULL;
    
    if (fs_sector_size == 0 || fs_pool_size == 0 || fs_bitmap == 0)
        return FS_NULL;
    
    total_size      = size + FS_HEADER_SIZE;
    required_blocks = fs_blocks_required(total_size);
    
    if (required_blocks == 0)
        return FS_NULL;
    
    if (required_blocks > (fs_block_count - fs_reserved_blocks))
        return FS_NULL;
    
    found_blocks = 0;
    start_block  = 0;
    
    for (current_block = fs_reserved_blocks; current_block < fs_block_count; current_block++) {
        if (!fs_block_is_used(current_block)) {
            if (found_blocks == 0)
                start_block = current_block;
            
            found_blocks++;
            
            if (found_blocks >= required_blocks) {
                uint32_t             block_offset;
                uint32_t             header_address;
                uint32_t             payload_address;
                struct FSHeaderBlock header;
                
                for (block_offset = 0; block_offset < required_blocks; block_offset++) {
                    fs_block_set_used(start_block + block_offset);
                }
                
                header_address  = start_block * fs_sector_size;
                payload_address = header_address + FS_HEADER_SIZE;
                
                header.size     = size;
                header.flags    = 0;
                header.type     = 0;
                header.reserved = 0;
                header.magic    = FS_MAGIC;
                
                fs_header_write(header_address, &header);
                
                return payload_address;
            }
        }
        else {
            found_blocks = 0;
        }
    }
    
    return FS_NULL;
}

void fs_free(uint32_t address) {
    uint32_t header_address;
    uint32_t total_size;
    uint32_t start_block;
    uint32_t required_blocks;
    uint32_t block_offset;
    struct FSHeaderBlock header;
    
    if (!fs_is_valid(address))
        return;
    
    header_address = address - FS_HEADER_SIZE;
    fs_header_read(header_address, &header);
    
    total_size      = header.size + FS_HEADER_SIZE;
    required_blocks = fs_blocks_required(total_size);
    start_block     = header_address / fs_sector_size;
    
    if (start_block < fs_reserved_blocks)
        return;
    
    if ((start_block + required_blocks) > fs_block_count)
        return;
    
    header.size     = 0;
    header.flags    = 0;
    header.type     = 0;
    header.reserved = 0;
    header.magic    = 0;
    
    fs_header_write(header_address, &header);
    
    for (block_offset = 0; block_offset < required_blocks; block_offset++) {
        fs_block_set_free(start_block + block_offset);
    }
}


uint8_t fs_get_flags(uint32_t address) {
    uint32_t header_address;
    struct FSHeaderBlock header;
    
    if (!fs_is_valid(address))
        return 0;
    
    header_address = address - FS_HEADER_SIZE;
    fs_header_read(header_address, &header);
    
    return header.flags;
}

void fs_set_flags(uint32_t address, uint8_t flags) {
    uint32_t header_address;
    struct FSHeaderBlock header;
    
    if (!fs_is_valid(address))
        return;
    
    header_address = address - FS_HEADER_SIZE;
    fs_header_read(header_address, &header);
    
    header.flags = flags;
    fs_header_write(header_address, &header);
}

uint8_t fs_get_type(uint32_t address) {
    uint32_t header_address;
    struct FSHeaderBlock header;
    
    if (!fs_is_valid(address))
        return 0;
    
    header_address = address - FS_HEADER_SIZE;
    fs_header_read(header_address, &header);
    
    return header.type;
}

void fs_set_type(uint32_t address, uint8_t type) {
    uint32_t header_address;
    struct FSHeaderBlock header;
    
    if (!fs_is_valid(address))
        return;
    
    header_address = address - FS_HEADER_SIZE;
    fs_header_read(header_address, &header);
    
    header.type = type;
    fs_header_write(header_address, &header);
}

uint32_t fs_get_size(uint32_t address) {
    uint32_t header_address;
    struct FSHeaderBlock header;
    
    if (!fs_is_valid(address))
        return 0;
    
    header_address = address - FS_HEADER_SIZE;
    fs_header_read(header_address, &header);
    
    return header.size;
}

void fs_mem_write(uint32_t address, const void* source, uint32_t size) {
    const uint8_t* bytes = (const uint8_t*)source;
    uint32_t       index;
    
    for (index = 0; index < size; index++) {
        fs_writeb(address + index, bytes[index]);
    }
}

void fs_mem_read(uint32_t address, void* destination, uint32_t size) {
    uint8_t* bytes = (uint8_t*)destination;
    uint32_t index;
    
    for (index = 0; index < size; index++) {
        bytes[index] = fs_read8(address + index);
    }
}

uint32_t fs_create_file(void* buffer, uint32_t size) {
    uint32_t device_address = fs_alloc(size);
    
    fs_set_flags(device_address, KMALLOC_FLAG_DEVICE | KMALLOC_FLAG_FS | KMALLOC_FLAG_READ | KMALLOC_FLAG_WRITE);
    
    fs_set_type(device_address, 0);
    
    fs_mem_write(device_address, buffer, size);
    return device_address;
}

void fs_destroy_file(uint32_t address) {
    fs_free(address);
}

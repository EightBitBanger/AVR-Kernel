#include <kernel/fs/fs.h>
#include <kernel/bus/bus.h>

#include <kernel/arch/avr/io.h>
#include <kernel/boot/avr/heap.h>

#include <kernel/delay.h>

#include <string.h>
#include <stdbool.h>

static uint32_t fs_device_address = 0;

static uint32_t fs_sector_size    = 0;
static uint32_t fs_pool_size      = 0;
static uint32_t fs_block_count    = 0;

static uint32_t fs_bitmap_size     = 0;
static uint32_t fs_reserved_blocks = 0;

uint8_t* fs_bitmap = 0;

struct Bus fs_bus;

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
    return sizeof(struct FSDeviceHeader) + sizeof(struct FSHeaderBlock);
}

static inline void fs_bitmap_set(uint32_t index) {
    fs_bitmap[index >> 3] |= (uint8_t)(1U << (index & 7U));
}

static inline void fs_bitmap_clear(uint32_t index) {
    fs_bitmap[index >> 3] &= (uint8_t)~(1U << (index & 7U));
}

static inline bool fs_bitmap_get(uint32_t index) {
    return (fs_bitmap[index >> 3] & (uint8_t)(1U << (index & 7U))) != 0;
}

void fs_writeb(uint32_t address, uint8_t byte) {
    mmio_writeb(&fs_bus, fs_device_address + address, &byte);
    _delay_ms(5);
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

static inline bool fs_address_valid(uint32_t address, uint32_t size) {
    if (address == FS_NULL)
        return false;

    if (size > fs_pool_size)
        return false;

    if (address > (fs_pool_size - size))
        return false;

    return true;
}

static bool fs_alloc_header_read_from_payload(uint32_t payload_address, struct FSAllocHeader* header) {
    uint32_t allocation_address;

    if (payload_address == FS_NULL)
        return false;

    if (payload_address < sizeof(struct FSAllocHeader))
        return false;

    allocation_address = payload_address - sizeof(struct FSAllocHeader);

    if (!fs_address_valid(allocation_address, sizeof(struct FSAllocHeader)))
        return false;

    fs_mem_read(allocation_address, header, sizeof(struct FSAllocHeader));

    if (header->size == 0)
        return false;

    return true;
}

static bool fs_directory_header_read(uint32_t directory_address, struct FSDirectoryHeader* directory) {
    struct FSAllocHeader alloc_header;

    if (!fs_alloc_header_read_from_payload(directory_address, &alloc_header))
        return false;

    if (alloc_header.size < sizeof(struct FSDirectoryHeader))
        return false;

    if (!fs_address_valid(directory_address, sizeof(struct FSDirectoryHeader)))
        return false;

    fs_mem_read(directory_address, directory, sizeof(struct FSDirectoryHeader));
    return true;
}

static void fs_directory_header_write(uint32_t directory_address, const struct FSDirectoryHeader* directory) {
    fs_mem_write(directory_address, directory, sizeof(struct FSDirectoryHeader));
}

static bool fs_directory_extent_read(uint32_t extent_address, struct FSDirectoryExtent* extent) {
    struct FSAllocHeader alloc_header;

    if (!fs_alloc_header_read_from_payload(extent_address, &alloc_header))
        return false;

    if (alloc_header.size < sizeof(struct FSDirectoryExtent))
        return false;

    if (!fs_address_valid(extent_address, sizeof(struct FSDirectoryExtent)))
        return false;

    fs_mem_read(extent_address, extent, sizeof(struct FSDirectoryExtent));
    return true;
}

static void fs_directory_extent_write(uint32_t extent_address, const struct FSDirectoryExtent* extent) {
    fs_mem_write(extent_address, extent, sizeof(struct FSDirectoryExtent));
}

static uint32_t fs_directory_extent_create(uint32_t prev_address) {
    struct FSDirectoryExtent extent;
    uint32_t                 extent_address;

    extent_address = fs_alloc(sizeof(struct FSDirectoryExtent));
    if (extent_address == FS_NULL)
        return FS_NULL;

    memset(&extent, 0x00, sizeof(struct FSDirectoryExtent));
    extent.prev = prev_address;
    extent.next = FS_NULL;
    extent.reference_count = 0;

    fs_directory_extent_write(extent_address, &extent);

    return extent_address;
}

static void fs_directory_extent_unlink_and_free(uint32_t directory_address, uint32_t extent_address) {
    struct FSDirectoryHeader directory;
    struct FSDirectoryExtent extent;
    struct FSDirectoryExtent prev_extent;
    struct FSDirectoryExtent next_extent;

    if (!fs_directory_header_read(directory_address, &directory))
        return;

    if (!fs_directory_extent_read(extent_address, &extent))
        return;

    if (extent.prev == directory_address) {
        directory.next = extent.next;
        fs_directory_header_write(directory_address, &directory);
    } else if (extent.prev != FS_NULL) {
        if (fs_directory_extent_read(extent.prev, &prev_extent)) {
            prev_extent.next = extent.next;
            fs_directory_extent_write(extent.prev, &prev_extent);
        }
    }

    if (extent.next != FS_NULL) {
        if (fs_directory_extent_read(extent.next, &next_extent)) {
            next_extent.prev = extent.prev;
            fs_directory_extent_write(extent.next, &next_extent);
        }
    }

    fs_free(extent_address);
}

uint32_t fs_device_get_bitmap_size(uint32_t device_address) {
    struct FSDeviceHeader deviceHeader;
    struct FSHeaderBlock  headerBlock;

    uint32_t current_device = fs_device_address;
    fs_device_address = device_address;

    fs_mem_read(0x00000, &deviceHeader, sizeof(struct FSDeviceHeader));

    if (deviceHeader.id != 0x13) {
        fs_device_address = current_device;
        return 0;
    }

    fs_mem_read(sizeof(struct FSDeviceHeader), &headerBlock, sizeof(struct FSHeaderBlock));

    fs_device_address = current_device;

    if (headerBlock.magic != FS_MAGIC)
        return 0;

    if (headerBlock.sector_size == 0 || headerBlock.total_size == 0)
        return 0;

    {
        uint32_t block_count = headerBlock.total_size / headerBlock.sector_size;

        if (block_count == 0)
            return 0;

        return (block_count + 7UL) / 8UL;
    }
}

uint8_t fs_device_open(uint32_t device_address, uint8_t* bitmap, struct FSHeaderBlock* header) {
    struct FSDeviceHeader deviceHeader;
    struct FSHeaderBlock  headerBlock;
    uint32_t              metadata_size;

    fs_device_address = device_address;

    fs_mem_read(0x00000, &deviceHeader, sizeof(struct FSDeviceHeader));
    fs_mem_read(sizeof(struct FSDeviceHeader), &headerBlock, sizeof(struct FSHeaderBlock));

    if (headerBlock.magic != FS_MAGIC)
        return 1;

    if (headerBlock.sector_size == 0)
        return 2;

    if (headerBlock.total_size == 0)
        return 3;

    fs_sector_size = headerBlock.sector_size;
    fs_pool_size   = headerBlock.total_size;
    fs_block_count = fs_pool_size / fs_sector_size;

    if (fs_block_count == 0)
        return 4;

    fs_bitmap_size = (fs_block_count + 7UL) / 8UL;

    metadata_size = sizeof(struct FSDeviceHeader)
                  + sizeof(struct FSHeaderBlock)
                  + fs_bitmap_size;

    fs_reserved_blocks = (metadata_size + fs_sector_size - 1UL) / fs_sector_size;

    if (bitmap != NULL) {
        fs_mem_read(fs_bitmap_address(), bitmap, fs_bitmap_size);
    }

    fs_bitmap = bitmap;

    *header = headerBlock;

    return 0;
}

void fs_device_format(uint32_t device_address, uint32_t capacity_max, uint32_t sector_size) {
    struct FSDeviceHeader deviceHeader;
    struct FSHeaderBlock  headerBlock;
    uint32_t              block_count;
    uint32_t              bitmap_size;
    uint32_t              metadata_size;
    uint32_t              reserved_blocks;
    uint32_t              index;

    if (sector_size == 0)
        return;

    block_count = capacity_max / sector_size;

    if (block_count == 0)
        return;

    fs_device_address = device_address;

    bitmap_size = (block_count + 7UL) / 8UL;

    metadata_size = sizeof(struct FSDeviceHeader)
                  + sizeof(struct FSHeaderBlock)
                  + bitmap_size;

    reserved_blocks = (metadata_size + sector_size - 1UL) / sector_size;

    memset(&deviceHeader, 0x00, sizeof(struct FSDeviceHeader));
    strcpy(deviceHeader.name, "fs");
    deviceHeader.id = 0x13;

    memset(&headerBlock, 0x00, sizeof(struct FSHeaderBlock));
    headerBlock.total_size     = capacity_max;
    headerBlock.sector_size    = sector_size;
    headerBlock.root_directory = FS_NULL;
    strcpy(headerBlock.name, "eeprom");
    headerBlock.flags = 0;
    headerBlock.type  = 0;
    headerBlock.magic = FS_MAGIC;

    fs_mem_write(0x00000, &deviceHeader, sizeof(struct FSDeviceHeader));
    fs_mem_write(sizeof(struct FSDeviceHeader), &headerBlock, sizeof(struct FSHeaderBlock));

    for (index = 0; index < bitmap_size; index++) {
        fs_write8(fs_bitmap_address() + index, 0x00);
    }

    for (index = 0; index < reserved_blocks; index++) {
        uint32_t byte_index = index >> 3;
        uint8_t  byte_value = fs_read8(fs_bitmap_address() + byte_index);

        byte_value |= (uint8_t)(1U << (index & 7U));

        fs_write8(fs_bitmap_address() + byte_index, byte_value);
    }

    fs_sector_size     = sector_size;
    fs_pool_size       = capacity_max;
    fs_block_count     = block_count;
    fs_bitmap_size     = bitmap_size;
    fs_reserved_blocks = reserved_blocks;
}

void fs_device_set_address(uint32_t device_address) {
    fs_device_address = device_address;
}

void fs_bitmap_flush(void) {
    if (fs_bitmap == NULL)
        return;

    if (fs_bitmap_size == 0)
        return;

    fs_mem_write(fs_bitmap_address(), fs_bitmap, fs_bitmap_size);
}

uint32_t fs_bitmap_get_size(void) {
    return fs_bitmap_size;
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

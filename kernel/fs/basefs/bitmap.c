#include <kernel/fs/fs.h>

extern struct FSDeviceContext device_context;

static inline uint32_t fs_bitmap_address(void) {
    return sizeof(struct FSDeviceHeader) + sizeof(struct FSPartitionBlock);
}

bool fs_bitmap_get(uint32_t index) {
    fs_bitmap_sync(index);
    uint32_t local_byte = (index >> 3) - device_context.frame_offset;
    return (device_context.frame_bitmap[local_byte] & (uint8_t)(1U << (index & 7U))) != 0;
}

void fs_bitmap_set(uint32_t index) {
    fs_bitmap_sync(index);
    uint32_t local_byte = (index >> 3) - device_context.frame_offset;
    uint8_t mask = (uint8_t)(1U << (index & 7U));
    if (!(device_context.frame_bitmap[local_byte] & mask)) {
        device_context.frame_bitmap[local_byte] |= mask;
        device_context.frame_dirty = true;
    }
}

void fs_bitmap_clear(uint32_t index) {
    fs_bitmap_sync(index);
    uint32_t local_byte = (index >> 3) - device_context.frame_offset;
    uint8_t mask = (uint8_t)(1U << (index & 7U));
    if (device_context.frame_bitmap[local_byte] & mask) {
        device_context.frame_bitmap[local_byte] &= ~mask;
        device_context.frame_dirty = true;
    }
}

void fs_bitmap_flush(void) {
    if (device_context.frame_dirty && device_context.frame_offset != 0xFFFFFFFF) {
        uint32_t write_size = BITMAP_FRAME_SIZE;
        if (device_context.frame_offset + write_size > device_context.bitmap_size) {
            write_size = device_context.bitmap_size - device_context.frame_offset;
        }
        fs_mem_write(fs_bitmap_address() + device_context.frame_offset, device_context.frame_bitmap, write_size);
        device_context.frame_dirty = false;
    }
}

void fs_bitmap_sync(uint32_t bit_index) {
    uint32_t byte_index = bit_index >> 3;
    uint32_t window_start = (byte_index / BITMAP_FRAME_SIZE) * BITMAP_FRAME_SIZE;
    
    if (device_context.frame_offset != window_start) {
        fs_bitmap_flush(); // Save current window if dirty
        
        device_context.frame_offset = window_start;
        uint32_t read_size = BITMAP_FRAME_SIZE;
        if (device_context.frame_offset + read_size > device_context.bitmap_size) {
            read_size = device_context.bitmap_size - device_context.frame_offset;
        }
        
        fs_mem_read(fs_bitmap_address() + device_context.frame_offset, device_context.frame_bitmap, read_size);
        device_context.frame_dirty = false;
    }
}

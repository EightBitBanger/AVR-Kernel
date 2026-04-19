#include <kernel/fs/fs.h>

extern uint8_t  fs_frame_bitmap[BITMAP_FRAME_SIZE];
extern uint32_t fs_frame_offset;
extern bool     fs_frame_dirty;

extern uint32_t fs_bitmap_size;

static inline uint32_t fs_bitmap_address(void) {
    return sizeof(struct FSDeviceHeader) + sizeof(struct FSPartitionBlock);
}

bool fs_bitmap_get(uint32_t index) {
    fs_bitmap_sync(index);
    uint32_t local_byte = (index >> 3) - fs_frame_offset;
    return (fs_frame_bitmap[local_byte] & (uint8_t)(1U << (index & 7U))) != 0;
}

void fs_bitmap_set(uint32_t index) {
    fs_bitmap_sync(index);
    uint32_t local_byte = (index >> 3) - fs_frame_offset;
    uint8_t mask = (uint8_t)(1U << (index & 7U));
    if (!(fs_frame_bitmap[local_byte] & mask)) {
        fs_frame_bitmap[local_byte] |= mask;
        fs_frame_dirty = true;
    }
}

void fs_bitmap_clear(uint32_t index) {
    fs_bitmap_sync(index);
    uint32_t local_byte = (index >> 3) - fs_frame_offset;
    uint8_t mask = (uint8_t)(1U << (index & 7U));
    if (fs_frame_bitmap[local_byte] & mask) {
        fs_frame_bitmap[local_byte] &= ~mask;
        fs_frame_dirty = true;
    }
}

void fs_bitmap_flush(void) {
    if (fs_frame_dirty && fs_frame_offset != 0xFFFFFFFF) {
        uint32_t write_size = BITMAP_FRAME_SIZE;
        if (fs_frame_offset + write_size > fs_bitmap_size) {
            write_size = fs_bitmap_size - fs_frame_offset;
        }
        fs_mem_write(fs_bitmap_address() + fs_frame_offset, fs_frame_bitmap, write_size);
        fs_frame_dirty = false;
    }
}

void fs_bitmap_sync(uint32_t bit_index) {
    uint32_t byte_index = bit_index >> 3;
    uint32_t window_start = (byte_index / BITMAP_FRAME_SIZE) * BITMAP_FRAME_SIZE;
    
    if (fs_frame_offset != window_start) {
        fs_bitmap_flush(); // Save current window if dirty
        
        fs_frame_offset = window_start;
        uint32_t read_size = BITMAP_FRAME_SIZE;
        if (fs_frame_offset + read_size > fs_bitmap_size) {
            read_size = fs_bitmap_size - fs_frame_offset;
        }
        
        fs_mem_read(fs_bitmap_address() + fs_frame_offset, fs_frame_bitmap, read_size);
        fs_frame_dirty = false;
    }
}

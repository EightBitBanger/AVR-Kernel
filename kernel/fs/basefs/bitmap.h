#ifndef BASE_BITMAP_H
#define BASE_BITMAP_H

#define BITMAP_FRAME_SIZE   32

bool fs_bitmap_get(uint32_t index);
void fs_bitmap_set(uint32_t index);
void fs_bitmap_clear(uint32_t index);

void fs_bitmap_sync(uint32_t bit_index);
void fs_bitmap_flush(void);

#endif

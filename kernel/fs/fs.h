#ifndef BASE_FILE_SYSTEM_H
#define BASE_FILE_SYSTEM_H

#include <kernel/bus/bus.h>
#include <kernel/util/delay.h>

#include <kernel/fs/basefs/io.h>
#include <kernel/fs/basefs/structs.h>
#include <kernel/fs/basefs/bitmap.h>
#include <kernel/fs/basefs/flags.h>

#include <kernel/fs/basefs/file.h>
#include <kernel/fs/basefs/directory.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define FS_MAGIC                0xAAUL
#define FS_NULL                 0xFFFFFFFFUL
#define FS_INVALID_FRAME        0xFFFFFFFFUL

void fs_init(void);

// Initiate the device to a given size
void fs_device_format(uint32_t device_address, uint32_t capacity_max, uint32_t sector_size, uint16_t device_type);

// Open a device for IO operations
uint8_t fs_device_open(uint32_t device_address, struct FSPartitionBlock* partition, uint16_t device_type);

// Get the number of used bytes on this device
uint32_t fs_get_used_bytes(void);

// Get the device partition block
uint8_t fs_device_get_partition(uint32_t device_address, struct FSPartitionBlock* part);

// Find raw allocations
uint32_t fs_find_next(uint32_t previous_address);

// Bitmap allocation tracking
void fs_bitmap_flush(void);
uint32_t fs_bitmap_get_size();

// Low level allocation
uint32_t fs_alloc(uint32_t size);
void fs_free(uint32_t address);

// File IO
void fs_mem_write(uint32_t address, const void* source, uint32_t size);
void fs_mem_read(uint32_t address, void* destination, uint32_t size);

// Check if the directory is valid
bool fs_check_directory_valid(uint32_t address);

// Flush the cache back to disk
void fs_cache_sync(void);

#endif

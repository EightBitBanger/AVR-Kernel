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

#include <stdint.h>
#include <stdbool.h>

#define FS_MAGIC                0xAAUL
#define FS_NULL                 0xFFFFFFFFUL
#define FS_INVALID_FRAME        0xFFFFFFFFUL

void fs_init(void);
void fs_device_format(uint32_t device_address, uint32_t capacity_max, uint32_t sector_size, uint16_t device_type);
void fs_device_format_low(uint32_t device_address, uint32_t capacity);

struct FSDeviceContext fs_device_open(uint32_t device_address, struct FSPartitionBlock* partition, uint16_t device_type);
uint32_t fs_get_used_bytes(struct FSDeviceContext* device_context);
uint8_t fs_device_get_partition(struct FSDeviceContext* device_context, struct FSPartitionBlock* part);

uint32_t fs_find_next(struct FSDeviceContext* device_context, uint32_t previous_address);
void fs_bitmap_flush(struct FSDeviceContext* ctx);
uint32_t fs_bitmap_get_size(void);

uint32_t fs_alloc(struct FSDeviceContext* device_context, uint32_t size);
void fs_free(struct FSDeviceContext* device_context, uint32_t address);
bool fs_check_directory_valid(struct FSDeviceContext* ctx, uint32_t address);

void fs_cache_sync(struct FSDeviceContext* ctx);

#endif

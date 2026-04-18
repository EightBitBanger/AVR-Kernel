#ifndef BASE_FILE_SYSTEM_H
#define BASE_FILE_SYSTEM_H

#include <kernel/fs/basefs/file.h>
#include <kernel/fs/basefs/directory.h>
#include <kernel/fs/basefs/structs.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define FS_MAGIC        0xAAUL
#define FS_NULL         0xFFFFFFFFUL

#define FS_PERMISSION_EXECUTE      0x01
#define FS_PERMISSION_READ         0x02
#define FS_PERMISSION_WRITE        0x04

#define FS_ATTRIBUTE_READONLY      0x01
#define FS_ATTRIBUTE_DIRECTORY     0x02
#define FS_ATTRIBUTE_EXTENT        0x04

typedef struct FSDirectoryHandle DirectoryHandle;

void fs_init(void);

bool fs_device_check(uint32_t address, uint32_t size);
void fs_device_format(uint32_t device_address, uint32_t capacity_max, uint32_t sector_size);
uint8_t fs_device_open(uint32_t device_address, uint8_t* bitmap, uint8_t* bitmap_dirty, struct FSPartitionBlock* partition);

uint8_t fs_device_get_partition_header(uint32_t device_address, struct FSPartitionBlock* part);

uint32_t fs_find_next(uint32_t previous_address);

void fs_bitmap_flush(void);
uint32_t fs_bitmap_get_size();

uint32_t fs_alloc(uint32_t size);
void fs_free(uint32_t address);

void fs_mem_write(uint32_t address, const void* source, uint32_t size);
void fs_mem_read(uint32_t address, void* destination, uint32_t size);

bool fs_is_directory(uint32_t address);

#endif

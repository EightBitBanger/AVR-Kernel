#ifndef BASE_FILE_SYSTEM_H
#define BASE_FILE_SYSTEM_H

#include <kernel/fs/basefs/file.h>

#include <stdint.h>
#include <stdlib.h>

#define FS_MAGIC        0xAAUL
#define FS_NULL         0xFFFFFFFFUL

#define FS_PERMISSION_EXECUTE      0x01
#define FS_PERMISSION_READ         0x02
#define FS_PERMISSION_WRITE        0x04

#define FS_ATTRIBUTE_READONLY      0x01
#define FS_ATTRIBUTE_DIRECTORY     0x02
#define FS_ATTRIBUTE_EXTENT        0x04

struct __attribute__((packed)) FSDeviceHeader {
    uint8_t id;
    char name[10];
};

struct __attribute__((packed)) FSHeaderBlock {
    uint32_t total_size;
    uint32_t sector_size;
    
    uint32_t root_directory;
    
    char name[16];
    
    uint8_t  flags;
    uint8_t  type;
    
    uint8_t  reserved;
    uint8_t  magic;
};

struct __attribute__((packed)) FSAllocHeader {
    uint32_t size;
};

struct __attribute__((packed)) FSFileHeader {
    char name[16];
    
    uint8_t  attributes;
    uint8_t  permissions;
    
    uint8_t padding[10];
};

struct __attribute__((packed)) FSDirectoryHeader {
    char name[16];
    
    uint8_t  attributes;
    uint8_t  permissions;
    
    uint32_t parent;
    
    uint32_t next;
    uint32_t prev;
    
    uint32_t reference_count;
    uint32_t reference[32];
};

struct __attribute__((packed)) FSDirectoryExtent {
    uint32_t next;
    uint32_t prev;
    
    uint32_t reference_count;
    uint32_t reference[32];
};

void fs_init(void);

void fs_device_format(uint32_t device_address, uint32_t capacity_max, uint32_t sector_size);

uint8_t fs_device_open(uint32_t device_address, uint8_t* bitmap, struct FSHeaderBlock* header);

uint32_t fs_file_create(const char* name, uint8_t permissions);
void fs_file_delete(uint32_t address);

uint32_t fs_directory_create(const char* name, uint8_t permissions);
void fs_directory_delete(uint32_t address);

uint8_t fs_directory_add_reference(uint32_t directory_address, uint32_t reference_address);
uint8_t fs_directory_remove_reference(uint32_t directory_address, uint32_t reference_address);
uint32_t fs_directory_get_reference(uint32_t directory_address, uint32_t index);

uint32_t fs_find_next(uint32_t previous_address);

void fs_device_set_address(uint32_t device_address);

void fs_bitmap_flush(void);
uint32_t fs_bitmap_get_size();

uint32_t fs_alloc(uint32_t size);
void fs_free(uint32_t address);

void fs_mem_write(uint32_t address, const void* source, uint32_t size);
void fs_mem_read(uint32_t address, void* destination, uint32_t size);

#endif

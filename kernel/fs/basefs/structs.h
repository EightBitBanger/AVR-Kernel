#ifndef BASE_FILE_STRUCTS_H
#define BASE_FILE_STRUCTS_H

#include <kernel/fs/config.h>

#include <stdint.h>
#include <stdbool.h>

struct __attribute__((packed)) FSDeviceHeader {
    uint8_t id;
    char name[10];
    uint8_t pad[5];
};

struct __attribute__((packed)) FSPartitionBlock {
    uint32_t total_size;
    uint32_t sector_size;
    
    uint32_t root_directory;
    
    char name[FS_NAME_LENGTH_MAX];
    
    uint8_t  flags;
    uint8_t  type;
    
    uint8_t  reserved;
    uint8_t  magic;
};

struct __attribute__((packed)) FSAllocHeader {
    uint32_t size;
};

struct __attribute__((packed)) FSExtent {
    uint32_t next;
    uint32_t prev;
};

struct __attribute__((packed)) FSBlockHeader {
    char    name[FS_NAME_LENGTH_MAX];
    
    uint8_t attributes;
    uint8_t permissions;
    
    uint8_t padding[6];               
    
    uint32_t security;
    uint32_t certificate;
};

struct __attribute__((packed)) FSFileHeader {
    struct FSBlockHeader block;
    uint32_t             total_logical_size;
    struct FSExtent      extent;
};

struct __attribute__((packed)) FSFileExtent {
    struct FSExtent extent;
    uint32_t         extent_data_size;
};

struct __attribute__((packed)) FSDirectoryHeader {
    struct FSBlockHeader block;
    
    uint32_t parent;
    
    struct FSExtent extent;
    
    uint32_t reference_count;
};

struct __attribute__((packed)) FSDirectoryExtent {
    struct FSExtent extent;
    
    uint32_t reference_count;
};

#endif

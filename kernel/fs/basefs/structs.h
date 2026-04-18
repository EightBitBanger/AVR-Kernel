#ifndef BASE_FILE_STRUCTS_H
#define BASE_FILE_STRUCTS_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define FS_DIRECTORY_REF_MAX     6
#define FS_DIRECTORY_EX_REF_MAX  12

struct __attribute__((packed)) FSDeviceHeader {
    uint8_t id;
    char name[10];
};

struct __attribute__((packed)) FSPartitionBlock {
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

struct __attribute__((packed)) FSBlockHeader {
    char    name[16];
    
    uint8_t attributes;
    uint8_t permissions;
};

struct __attribute__((packed)) FSFileHeader {
    struct FSBlockHeader block;
    
    uint8_t padding[10];
};

struct __attribute__((packed)) FSDirectoryReference {
    uint32_t ref;
};

struct __attribute__((packed)) FSDirectoryHeader {
    struct FSBlockHeader block;
    
    uint32_t parent;
    
    uint32_t next;
    uint32_t prev;
    
    uint32_t reference_count;
    struct FSDirectoryReference reference[FS_DIRECTORY_REF_MAX];
    
    uint8_t padding[2];
};

struct __attribute__((packed)) FSDirectoryExtent {
    uint32_t next;
    uint32_t prev;
    
    uint32_t reference_count;
    struct FSDirectoryReference reference[FS_DIRECTORY_EX_REF_MAX];
};

#endif
